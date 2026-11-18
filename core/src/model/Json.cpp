#include "ih1/model/Json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace ih1::json {

void Value::set(const std::string& k, Value v) {
    if (type_ != Type::Object) { type_ = Type::Object; obj_.clear(); keys_.clear(); }
    if (!obj_.count(k)) keys_.push_back(k);
    obj_[k] = std::move(v);
}

const Value& Value::get(const std::string& k) const {
    static const Value n;
    if (!isObject()) return n;
    auto it = obj_.find(k);
    return it == obj_.end() ? n : it->second;
}

static void escape(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); out += b; }
                else out += static_cast<char>(c);
        }
    }
    out += '"';
}

void Value::dumpTo(std::string& out, int indent, int level) const {
    auto nl = [&](int lv) {
        if (indent <= 0) return;
        out += '\n';
        out.append(static_cast<size_t>(indent * lv), ' ');
    };
    switch (type_) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += b_ ? "true" : "false"; break;
        case Type::Number: {
            if (!std::isfinite(d_)) { out += "0"; break; }
            char b[40];
            if (d_ == std::floor(d_) && std::fabs(d_) < 1e15) std::snprintf(b, sizeof b, "%.0f", d_);
            else std::snprintf(b, sizeof b, "%.9g", d_);
            out += b;
            break;
        }
        case Type::String: escape(out, s_); break;
        case Type::Array:
            out += '[';
            for (size_t i = 0; i < arr_.size(); ++i) {
                if (i) out += ',';
                nl(level + 1);
                arr_[i].dumpTo(out, indent, level + 1);
            }
            if (!arr_.empty()) nl(level);
            out += ']';
            break;
        case Type::Object:
            out += '{';
            for (size_t i = 0; i < keys_.size(); ++i) {
                if (i) out += ',';
                nl(level + 1);
                escape(out, keys_[i]);
                out += indent > 0 ? ": " : ":";
                obj_.at(keys_[i]).dumpTo(out, indent, level + 1);
            }
            if (!keys_.empty()) nl(level);
            out += '}';
            break;
    }
}

std::string Value::dump(int indent) const {
    std::string s;
    dumpTo(s, indent, 0);
    return s;
}

namespace {
struct Parser {
    const std::string& t;
    size_t i = 0;
    std::string err;
    int depth = 0;

    void ws() { while (i < t.size() && (t[i] == ' ' || t[i] == '\n' || t[i] == '\r' || t[i] == '\t')) ++i; }
    bool fail(const char* m) { if (err.empty()) err = std::string(m) + " at offset " + std::to_string(i); return false; }

    bool lit(const char* s) {
        size_t k = 0;
        while (s[k]) { if (i + k >= t.size() || t[i + k] != s[k]) return false; ++k; }
        i += k;
        return true;
    }

    bool str(std::string& out) {
        if (i >= t.size() || t[i] != '"') return fail("expected string");
        ++i;
        while (i < t.size()) {
            char c = t[i++];
            if (c == '"') return true;
            if (c == '\\') {
                if (i >= t.size()) return fail("bad escape");
                char e = t[i++];
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        if (i + 4 > t.size()) return fail("bad \\u");
                        unsigned cp = static_cast<unsigned>(std::strtoul(t.substr(i, 4).c_str(), nullptr, 16));
                        i += 4;
                        if (cp < 0x80) out += static_cast<char>(cp);
                        else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
                        else { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
                        break;
                    }
                    default: return fail("bad escape");
                }
            } else if (static_cast<unsigned char>(c) < 0x20) {
                return fail("control char in string");
            } else {
                out += c;
            }
        }
        return fail("unterminated string");
    }

    bool value(Value& v) {
        if (++depth > 64) return fail("too deep");
        ws();
        if (i >= t.size()) return fail("unexpected end");
        char c = t[i];
        bool ok = true;
        if (c == '{') {
            ++i;
            v = Value::object();
            ws();
            if (i < t.size() && t[i] == '}') { ++i; }
            else {
                for (;;) {
                    ws();
                    std::string k;
                    if (!str(k)) { ok = false; break; }
                    ws();
                    if (i >= t.size() || t[i] != ':') { ok = fail("expected ':'"); break; }
                    ++i;
                    Value child;
                    if (!value(child)) { ok = false; break; }
                    v.set(k, std::move(child));
                    ws();
                    if (i < t.size() && t[i] == ',') { ++i; continue; }
                    if (i < t.size() && t[i] == '}') { ++i; break; }
                    ok = fail("expected ',' or '}'");
                    break;
                }
            }
        } else if (c == '[') {
            ++i;
            v = Value::array();
            ws();
            if (i < t.size() && t[i] == ']') { ++i; }
            else {
                for (;;) {
                    Value child;
                    if (!value(child)) { ok = false; break; }
                    v.push(std::move(child));
                    ws();
                    if (i < t.size() && t[i] == ',') { ++i; continue; }
                    if (i < t.size() && t[i] == ']') { ++i; break; }
                    ok = fail("expected ',' or ']'");
                    break;
                }
            }
        } else if (c == '"') {
            std::string s;
            ok = str(s);
            v = Value(std::move(s));
        } else if (lit("true")) { v = Value(true); }
        else if (lit("false")) { v = Value(false); }
        else if (lit("null")) { v = Value(); }
        else if (c == '-' || (c >= '0' && c <= '9')) {
            const char* start = t.c_str() + i;
            char* end = nullptr;
            double d = std::strtod(start, &end);
            if (end == start) ok = fail("bad number");
            else { i += static_cast<size_t>(end - start); v = Value(d); }
        } else {
            ok = fail("unexpected character");
        }
        --depth;
        return ok;
    }
};
}  // namespace

bool parse(const std::string& text, Value& out, std::string* error) {
    if (text.size() > (16u << 20)) { if (error) *error = "state too large"; return false; }
    Parser p{text, 0, {}, 0};
    Value v;
    bool ok = p.value(v);
    if (ok) { p.ws(); if (p.i != text.size()) ok = p.fail("trailing data"); }
    if (!ok) { if (error) *error = p.err; return false; }
    out = std::move(v);
    return true;
}

}  // namespace ih1::json
