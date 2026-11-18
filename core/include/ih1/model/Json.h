// Minimal JSON value, writer and strict parser for IH1 state (no external dependency).
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ih1::json {

class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Value() = default;
    Value(std::nullptr_t) {}
    Value(bool b) : type_(Type::Bool), b_(b) {}
    Value(double d) : type_(Type::Number), d_(d) {}
    Value(float d) : type_(Type::Number), d_(d) {}
    Value(int i) : type_(Type::Number), d_(i) {}
    Value(const char* s) : type_(Type::String), s_(s) {}
    Value(std::string s) : type_(Type::String), s_(std::move(s)) {}

    static Value array() { Value v; v.type_ = Type::Array; return v; }
    static Value object() { Value v; v.type_ = Type::Object; return v; }

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isBool() const { return type_ == Type::Bool; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    bool asBool(bool def = false) const { return isBool() ? b_ : def; }
    double asNumber(double def = 0.0) const { return isNumber() ? d_ : def; }
    const std::string& asString() const { static const std::string e; return isString() ? s_ : e; }

    // arrays
    void push(Value v) { type_ = Type::Array; arr_.push_back(std::move(v)); }
    size_t size() const { return isArray() ? arr_.size() : (isObject() ? obj_.size() : 0); }
    const Value& operator[](size_t i) const { static const Value n; return (isArray() && i < arr_.size()) ? arr_[i] : n; }
    const std::vector<Value>& items() const { return arr_; }

    // objects (key order preserved by insertion via separate key list)
    void set(const std::string& k, Value v);
    const Value& get(const std::string& k) const;
    bool has(const std::string& k) const { return isObject() && obj_.count(k) != 0; }
    const std::vector<std::string>& keys() const { return keys_; }

    std::string dump(int indent = 0) const;

private:
    void dumpTo(std::string& out, int indent, int level) const;
    Type type_ = Type::Null;
    bool b_ = false;
    double d_ = 0.0;
    std::string s_;
    std::vector<Value> arr_;
    std::map<std::string, Value> obj_;
    std::vector<std::string> keys_;
};

// Strict parser. Returns false and fills `error` on malformed input. Depth and size are bounded.
bool parse(const std::string& text, Value& out, std::string* error = nullptr);

}  // namespace ih1::json
