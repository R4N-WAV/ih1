#include "ih1/model/Session.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>

#include "ih1/model/Json.h"

namespace ih1 {

namespace {
uint64_t splitmix(uint64_t& s) {
    uint64_t z = (s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
constexpr const char* kFormat = "ih1-state";
constexpr int kStateVersion = 1;
}  // namespace

std::string idToHex(uint64_t id) {
    char b[17];
    std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(id));
    return b;
}

uint64_t idFromHex(const std::string& s) {
    if (s.empty() || s.size() > 16) return 0;
    uint64_t v = 0;
    for (char c : s) {
        v <<= 4;
        if (c >= '0' && c <= '9') v |= static_cast<uint64_t>(c - '0');
        else if (c >= 'a' && c <= 'f') v |= static_cast<uint64_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= static_cast<uint64_t>(c - 'A' + 10);
        else return 0;
    }
    return v;
}

Session::Session(Engine& engine) : engine_(engine) {
    const auto& reg = fxRegistry();
    drafts_.reserve(reg.size());
    for (const auto& d : reg) drafts_.push_back(d.defaults());
    std::random_device rd;
    idState_ = (static_cast<uint64_t>(rd()) << 32) ^ rd() ^
               static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    sampleRate_ = engine_.sampleRate();
    if (engine_.maxBlock() > 0) maxBlock_ = engine_.maxBlock();
}

Session::~Session() {
    // Detach everything from the engine (audio must be stopped by now), then free.
    if (pending_) { releaseProgram(pending_); pending_ = nullptr; }
    std::vector<Program*> released;
    engine_.installDirect(new Program(), released);
    for (Program* p : released) releaseProgram(p);
    chain_.clear();
    staged_ = nullptr;
    owned_.clear();
}

uint64_t Session::newId() {
    for (;;) {
        const uint64_t id = splitmix(idState_);
        if (id != 0 && indexOf(id) < 0) return id;
    }
}

std::unique_ptr<FxProcessor> Session::makeProcessor(int typeIndex, const ParamValues& values) {
    const auto& d = fxRegistry()[static_cast<size_t>(typeIndex)];
    auto p = d.create();
    p->setLatencyMode(d.resolveLatency(latencyMode_));
    p->prepare(sampleRate_, maxBlock_);
    p->setParams(d.clamp(values), true);
    return p;
}

FxProcessor* Session::adopt(std::unique_ptr<FxProcessor> p) {
    FxProcessor* raw = p.get();
    owned_.emplace(raw, std::move(p));
    return raw;
}

void Session::prepare(double sampleRate, int maxBlock) {
    sampleRate_ = sampleRate > 0 ? sampleRate : 48000.0;
    maxBlock_ = std::max(1, maxBlock);
    engine_.prepare(sampleRate_, maxBlock_);
    for (auto& kv : owned_) kv.first->prepare(sampleRate_, maxBlock_);
    for (const auto& ci : chain_) ci.processor()->setParams(ci.values(), true);
    if (staged_ && selected_ >= 0) staged_->setParams(drafts_[static_cast<size_t>(selected_)], true);

    if (pending_) { releaseProgram(pending_); pending_ = nullptr; }
    rebuild(false, false);
    Program* p = pending_;
    pending_ = nullptr;
    std::vector<Program*> released;
    engine_.installDirect(p, released);
    for (Program* r : released) releaseProgram(r);
    if (selected_ >= 0) engine_.publishStagedParams(selected_, drafts_[static_cast<size_t>(selected_)]);
    syncRouting();
    collectGarbage();
}

void Session::rebuild(bool commitHandover, bool post) {
    auto* p = new Program();
    p->chainSize = static_cast<int>(chain_.size());
    for (size_t i = 0; i < chain_.size(); ++i) {
        p->chain[i] = chain_[i].processor();
        p->ids[i] = chain_[i].id();
        p->chainValues[i] = chain_[i].values();
        p->chainLatency += chain_[i].processor()->latencySamples();
        ++refs_[p->chain[i]];
    }
    p->staged = staged_;
    p->stagedType = staged_ ? selected_ : -1;
    p->stagedLatency = staged_ ? staged_->latencySamples() : 0;
    if (staged_) ++refs_[staged_];
    p->commitHandover = commitHandover;
    if (pending_) releaseProgram(pending_);  // superseded before the audio thread ever saw it
    pending_ = p;
    ++revision_;
    if (post) tryPost();
}

void Session::tryPost() {
    if (pending_ && engine_.post(pending_)) pending_ = nullptr;
}

void Session::releaseProgram(Program* p) {
    if (!p) return;
    for (int i = 0; i < p->chainSize; ++i) {
        auto it = refs_.find(p->chain[static_cast<size_t>(i)]);
        if (it != refs_.end() && --it->second <= 0) refs_.erase(it);
    }
    if (p->staged) {
        auto it = refs_.find(p->staged);
        if (it != refs_.end() && --it->second <= 0) refs_.erase(it);
    }
    delete p;
}

void Session::collectGarbage() {
    std::unordered_set<FxProcessor*> live;
    for (const auto& ci : chain_) live.insert(ci.processor());
    if (staged_) live.insert(staged_);
    for (auto it = owned_.begin(); it != owned_.end();) {
        if (!live.count(it->first) && !refs_.count(it->first)) it = owned_.erase(it);
        else ++it;
    }
}

void Session::service() {
    Program* r = nullptr;
    while (engine_.popRetired(r)) releaseProgram(r);
    tryPost();
    collectGarbage();
}

void Session::syncRouting() {
    engine_.setSolo(solo_);
    engine_.setMute(mute_);
}

// ---- staged ------------------------------------------------------------------------------

void Session::select(int typeIndex) {
    const int n = static_cast<int>(fxRegistry().size());
    if (typeIndex < -1 || typeIndex >= n) typeIndex = -1;
    if (typeIndex == selected_) return;
    selected_ = typeIndex;
    staged_ = typeIndex >= 0 ? adopt(makeProcessor(typeIndex, drafts_[static_cast<size_t>(typeIndex)])) : nullptr;
    if (!staged_) { solo_ = false; mute_ = false; syncRouting(); }
    rebuild(false);
    if (typeIndex >= 0) engine_.publishStagedParams(typeIndex, drafts_[static_cast<size_t>(typeIndex)]);
    collectGarbage();
}

void Session::step(int direction) {
    const auto& order = fxCarouselOrder();
    if (order.empty()) return;
    const int n = static_cast<int>(order.size());
    int pos = -1;
    for (int i = 0; i < n; ++i) if (order[static_cast<size_t>(i)] == selected_) pos = i;
    if (pos < 0) pos = direction >= 0 ? 0 : n - 1;  // from "nothing selected" land on first/last
    else pos = ((pos + (direction >= 0 ? 1 : -1)) % n + n) % n;
    select(order[static_cast<size_t>(pos)]);
}

void Session::setDraftParam(int paramIndex, float plainValue) {
    if (selected_ < 0) return;
    const auto& d = fxRegistry()[static_cast<size_t>(selected_)];
    if (paramIndex < 0 || paramIndex >= static_cast<int>(d.params.size())) return;
    auto& v = drafts_[static_cast<size_t>(selected_)];
    v[static_cast<size_t>(paramIndex)] = d.params[static_cast<size_t>(paramIndex)].clamp(plainValue);
    engine_.publishStagedParams(selected_, v);
    ++revision_;
}

void Session::resetDraft() {
    if (selected_ < 0) return;
    drafts_[static_cast<size_t>(selected_)] = fxRegistry()[static_cast<size_t>(selected_)].defaults();
    // RESET also clears everything the staged processor has accumulated (Looper recording, delay and
    // reverb memory, envelopes): a freshly prepared processor replaces it through the normal program
    // hand-over (built here on the message thread, crossfaded in on the audio thread, the old one retired
    // and freed here later). Committed instances are separate processors and are never touched.
    staged_ = adopt(makeProcessor(selected_, drafts_[static_cast<size_t>(selected_)]));
    rebuild(false);
    engine_.publishStagedParams(selected_, drafts_[static_cast<size_t>(selected_)]);
    collectGarbage();
    ++revision_;
}

bool Session::commit() {
    if (selected_ < 0 || !staged_ || static_cast<int>(chain_.size()) >= kMaxChain) return false;
    const uint64_t id = newId();
    uint64_t seedState = id;
    const auto seed = static_cast<uint32_t>(splitmix(seedState));
    chain_.emplace_back(id, selected_, fxRegistry()[static_cast<size_t>(selected_)].clamp(drafts_[static_cast<size_t>(selected_)]), seed, staged_);
    // the draft stays as it is (decision B2); the carousel selection clears
    staged_ = nullptr;
    selected_ = -1;
    solo_ = mute_ = false;
    syncRouting();
    rebuild(true);
    return true;
}

// ---- chain -------------------------------------------------------------------------------

int Session::indexOf(uint64_t id) const {
    for (size_t i = 0; i < chain_.size(); ++i) if (chain_[i].id() == id) return static_cast<int>(i);
    return -1;
}

bool Session::move(int from, int to) {
    const int n = static_cast<int>(chain_.size());
    if (from < 0 || from >= n || to < 0 || to >= n) return false;
    if (from == to) return true;
    CommittedInstance ci = chain_[static_cast<size_t>(from)];
    chain_.erase(chain_.begin() + from);
    chain_.insert(chain_.begin() + to, ci);
    rebuild(false);
    return true;
}

bool Session::remove(uint64_t id) {
    const int i = indexOf(id);
    if (i < 0) return false;
    chain_.erase(chain_.begin() + i);
    rebuild(false);
    collectGarbage();
    return true;
}

void Session::clearChain() {
    chain_.clear();
    rebuild(false);
    collectGarbage();
}

void Session::initialise() {
    chain_.clear();
    selected_ = kNone;
    staged_ = nullptr;
    solo_ = mute_ = false;
    const auto& reg = fxRegistry();
    for (size_t t = 0; t < reg.size(); ++t) drafts_[t] = reg[t].defaults();
    syncRouting();
    rebuild(false);
    collectGarbage();
}

void Session::setSolo(bool on) {
    if (on && !staged_) on = false;  // nothing to audition
    solo_ = on;
    if (on) mute_ = false;
    syncRouting();
    ++revision_;
}

void Session::setMute(bool on) {
    if (on && !staged_) on = false;
    mute_ = on;
    if (on) solo_ = false;
    syncRouting();
    ++revision_;
}

void Session::setLatencyMode(LatencyMode m) {
    if (m == latencyMode_) return;
    latencyMode_ = m;
    // fresh processors of the resolved variant, same ids / frozen values / seeds; the old ones are retired
    // through the program queue and freed here once the audio thread has let go of them
    std::vector<CommittedInstance> next;
    next.reserve(chain_.size());
    for (const auto& ci : chain_)
        next.emplace_back(ci.id(), ci.typeIndex(), ci.values(), ci.visualSeed(), adopt(makeProcessor(ci.typeIndex(), ci.values())));
    chain_.swap(next);
    if (selected_ >= 0) staged_ = adopt(makeProcessor(selected_, drafts_[static_cast<size_t>(selected_)]));
    rebuild(false);
    if (selected_ >= 0) engine_.publishStagedParams(selected_, drafts_[static_cast<size_t>(selected_)]);
    collectGarbage();
}

std::vector<Session::LatencyContributor> Session::latencyContributors() const {
    std::vector<LatencyContributor> v;
    for (const auto& ci : chain_) {
        const int l = ci.processor()->latencySamples();
        if (l > 0) v.push_back({ci.id(), ci.typeIndex(), l, ci.descriptor().resolveLatency(latencyMode_), false});
    }
    if (staged_ && selected_ >= 0) {
        const int l = staged_->latencySamples();
        if (l > 0) v.push_back({0, selected_, l, fxRegistry()[static_cast<size_t>(selected_)].resolveLatency(latencyMode_), true});
    }
    return v;
}

int Session::latencySamples() const {
    int l = 0;
    for (const auto& ci : chain_) l += ci.processor()->latencySamples();
    if (staged_) l += staged_->latencySamples();
    return l;
}

double Session::tailSeconds() const {
    double t = 0.0;
    for (const auto& ci : chain_) t += ci.processor()->tailSeconds();
    if (staged_) t += staged_->tailSeconds();
    return std::min(t, 120.0);
}

// ---- state ---------------------------------------------------------------------------------

static json::Value paramsToJson(const FxDescriptor& d, const ParamValues& v) {
    json::Value o = json::Value::object();
    for (size_t i = 0; i < d.params.size(); ++i) o.set(d.params[i].id, json::Value(static_cast<double>(v[i])));
    return o;
}

static ParamValues paramsFromJson(const FxDescriptor& d, const json::Value& o) {
    ParamValues v = d.defaults();
    if (!o.isObject()) return v;
    for (size_t i = 0; i < d.params.size(); ++i) {
        const auto& x = o.get(d.params[i].id);
        if (x.isNumber()) v[i] = d.params[i].clamp(static_cast<float>(x.asNumber()));
    }
    return v;
}

std::string Session::toJson(bool includeSession) const {
    const auto& reg = fxRegistry();
    json::Value root = json::Value::object();
    root.set("format", kFormat);
    root.set("version", kStateVersion);
    json::Value chain = json::Value::array();
    for (const auto& ci : chain_) {
        const auto& d = ci.descriptor();
        json::Value e = json::Value::object();
        e.set("id", idToHex(ci.id()));
        e.set("type", d.typeId);
        e.set("fxVersion", d.version);
        e.set("seed", static_cast<double>(ci.visualSeed()));
        e.set("params", paramsToJson(d, ci.values()));
        chain.push(std::move(e));
    }
    root.set("chain", std::move(chain));
    if (includeSession) {
        json::Value s = json::Value::object();
        json::Value drafts = json::Value::object();
        for (size_t t = 0; t < reg.size(); ++t) {
            json::Value dd = json::Value::object();
            dd.set("fxVersion", reg[t].version);
            dd.set("params", paramsToJson(reg[t], drafts_[t]));
            drafts.set(reg[t].typeId, std::move(dd));
        }
        s.set("drafts", std::move(drafts));
        s.set("selected", selected_ >= 0 ? json::Value(reg[static_cast<size_t>(selected_)].typeId) : json::Value());
        s.set("solo", solo_);
        s.set("mute", mute_);
        s.set("latencyMode", static_cast<int>(latencyMode_));
        root.set("session", std::move(s));
    }
    return root.dump(1);
}

bool Session::fromJson(const std::string& text, bool includeSession, std::string* error) {
    json::Value root;
    std::string err;
    if (!json::parse(text, root, &err)) { if (error) *error = "parse error: " + err; return false; }
    if (!root.isObject() || root.get("format").asString() != kFormat) { if (error) *error = "not an IH1 state"; return false; }
    const int version = static_cast<int>(root.get("version").asNumber(0));
    if (version < 1 || version > kStateVersion) { if (error) *error = "unsupported state version " + std::to_string(version); return false; }
    const auto& chainJ = root.get("chain");
    if (!chainJ.isArray()) { if (error) *error = "missing chain"; return false; }

    // ---- validate and build everything before touching the live state ----
    struct NewInst { uint64_t id; int type; ParamValues v; uint32_t seed; };
    std::vector<NewInst> insts;
    std::unordered_set<uint64_t> seen;
    std::string warnings;
    for (const auto& e : chainJ.items()) {
        if (static_cast<int>(insts.size()) >= kMaxChain) { warnings += "chain truncated; "; break; }
        if (!e.isObject()) continue;
        const int t = fxTypeIndex(e.get("type").asString());
        if (t < 0) { warnings += "unknown FX '" + e.get("type").asString() + "' skipped; "; continue; }
        const auto& d = fxRegistry()[static_cast<size_t>(t)];
        uint64_t id = idFromHex(e.get("id").asString());
        if (id == 0 || seen.count(id)) id = 0;  // re-issued below
        NewInst ni{id, t, paramsFromJson(d, e.get("params")), static_cast<uint32_t>(e.get("seed").asNumber(0))};
        if (id) seen.insert(id);
        insts.push_back(ni);
    }
    std::vector<ParamValues> newDrafts = drafts_;
    int newSelected = -1;
    bool newSolo = false, newMute = false;
    LatencyMode newLatency = latencyMode_;
    if (includeSession) {
        const auto& s = root.get("session");
        if (s.isObject()) {
            const auto& dj = s.get("drafts");
            const auto& reg = fxRegistry();
            for (size_t t = 0; t < reg.size(); ++t) {
                newDrafts[t] = reg[t].defaults();
                const auto& one = dj.get(reg[t].typeId);
                if (one.isObject()) newDrafts[t] = paramsFromJson(reg[t], one.get("params"));
            }
            if (s.get("selected").isString()) newSelected = fxTypeIndex(s.get("selected").asString());
            newSolo = s.get("solo").asBool();
            newMute = s.get("mute").asBool() && !newSolo;
            // older states have no latency mode: Full Quality (the V1 behaviour)
            const int lm = static_cast<int>(s.get("latencyMode").asNumber(0));
            newLatency = static_cast<LatencyMode>(std::clamp(lm, 0, 2));
        }
    }

    // ---- apply ----
    if (includeSession) latencyMode_ = newLatency;  // before any processor is built
    chain_.clear();
    staged_ = nullptr;
    for (auto& ni : insts) {
        uint64_t id = ni.id;
        if (!id) id = newId();
        uint32_t seed = ni.seed;
        if (!seed) { uint64_t st = id; seed = static_cast<uint32_t>(splitmix(st)); }
        FxProcessor* p = adopt(makeProcessor(ni.type, ni.v));
        chain_.emplace_back(id, ni.type, ni.v, seed, p);
    }
    // V3 Phase 1: a recall always lands on None. The old staged processor is retired with the old program (crossfaded
    // out by the engine's program hand-over), so the recalled chain is heard alone. Drafts are a separate cache.
    (void)newSelected; (void)newSolo; (void)newMute;
    if (includeSession) drafts_ = newDrafts;
    selected_ = kNone;
    staged_ = nullptr;
    solo_ = mute_ = false;
    syncRouting();
    rebuild(false);
    collectGarbage();
    if (error) *error = warnings;
    return true;
}

}  // namespace ih1
