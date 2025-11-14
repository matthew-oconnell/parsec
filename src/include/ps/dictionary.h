#pragma once

#include <cstdint>
#include <string>
#include <map>
#include <variant>
#include <vector>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <ostream>
#include <optional>
#include <functional>

namespace ps {

struct Dictionary; // forward

struct Value {
    using list_t = std::vector<Value>;
    using dict_ptr = std::shared_ptr<Dictionary>;
    using key_type = std::string;

    std::variant<std::monostate, int64_t, double, bool, std::string, list_t, dict_ptr> v;

    Value() = default;
    Value(int64_t x) : v(x) {}
    Value(int x) : v(int64_t(x)) {}
    Value(double x) : v(x) {}
    Value(bool b) : v(b) {}
    Value(const char* s) : v(std::string(s)) {}
    Value(const std::string& s) : v(s) {}
    Value(std::string&& s) : v(std::move(s)) {}
    Value(const list_t& l) : v(l) {}
    Value(list_t&& l) : v(std::move(l)) {}
    Value(const Dictionary& d);
    Value(Dictionary&& d);

    // convenience constructors for common vector types
    Value(const std::vector<int>& v_) { list_t L; for (auto x: v_) L.emplace_back(int64_t(x)); v = std::move(L); }
    Value(const std::vector<double>& v_) { list_t L; for (auto x: v_) L.emplace_back(x); v = std::move(L); }
    Value(const std::vector<std::string>& v_) { list_t L; for (auto const &x: v_) L.emplace_back(x); v = std::move(L); }
    Value(const std::vector<bool>& v_) { list_t L; for (auto x: v_) L.emplace_back(x); v = std::move(L); }
    // accept a vector of Dictionary objects (useful for parsers that build object lists)
    Value(const std::vector<Dictionary>& v_);
    Value(std::vector<Dictionary>&& v_);

    // initializer-list helpers for convenient braced initialization
    // Note: avoid an initializer_list<std::string> overload to prevent
    // ambiguity with initializer_list<const char*> and initializer_list<Value>.
    Value(std::initializer_list<const char*> init) {
        list_t L; L.reserve(init.size());
        for (auto const &e: init) L.emplace_back(std::string(e));
        v = std::move(L);
    }
    // declared here, defined after Dictionary is declared
    Value(std::initializer_list<std::pair<const std::string, Value>> init);

    // canonical camelCase type checks (use std::holds_alternative directly)
    bool isNull() const noexcept { return std::holds_alternative<std::monostate>(v); }
    bool isInt() const noexcept { return std::holds_alternative<int64_t>(v); }
    bool isDouble() const noexcept { return std::holds_alternative<double>(v); }
    bool isBool() const noexcept { return std::holds_alternative<bool>(v); }
    bool isString() const noexcept { return std::holds_alternative<std::string>(v); }
    bool isList() const noexcept { return std::holds_alternative<list_t>(v); }
    bool isDict() const noexcept { return std::holds_alternative<dict_ptr>(v); }
    bool isValueObject() const { return not isList() and not isDict() and not isNull(); }
    bool isArrayObject() const { return isList(); }

    // underscore-style accessors removed. Use camelCase methods below.

    // legacy aliases (more tolerant conversions)
    std::string asString() const {
        if (isString()) return std::get<std::string>(v);
        return to_string();
    }
    int64_t asInt() const {
        if (isInt()) return std::get<int64_t>(v);
        if (isDouble()) return static_cast<int64_t>(asDouble());
        if (isBool()) return asBool() ? 1 : 0;
        if (isString()) { std::istringstream ss(asString()); int64_t vv; if (ss >> vv) return vv; }
        throw std::runtime_error("not an int");
    }
    double asDouble() const {
        if (isDouble()) return std::get<double>(v);
        if (isInt()) return static_cast<double>(asInt());
        if (isBool()) return asBool() ? 1.0 : 0.0;
        if (isString()) { std::istringstream ss(asString()); double vv; if (ss >> vv) return vv; }
        throw std::runtime_error("not a double");
    }
    bool asBool() const {
        if (isBool()) return std::get<bool>(v);
        if (isInt()) return asInt() != 0;
        if (isDouble()) return asDouble() != 0.0;
        if (isString()) {
            auto s = asString(); if (s == "true" or s == "1") return true; if (s == "false" or s == "0") return false; return not s.empty();
        }
        throw std::runtime_error("not a bool");
    }

    // camelCase wrappers for collection accessors (direct variant access)
    const list_t& asList() const { return std::get<list_t>(v); }
    const dict_ptr& asDict() const { return std::get<dict_ptr>(v); }

    bool operator==(const Value& o) const noexcept { return v == o.v; }

    // array conversion helpers
    std::vector<int> asInts() const;
    std::vector<double> asDoubles() const;
    std::vector<std::string> asStrings() const;
    std::vector<bool> asBools() const;
    std::vector<Dictionary> asObjects() const;

    // indexing and access
    Value& operator[](size_t idx);
    const Value& operator[](size_t idx) const;
    Value& operator[](const key_type& k);

    const Value& at(size_t idx) const;
    Value& at(size_t idx);
    const Value& at(const key_type& k) const;
    Value& at(const key_type& k);

    int size() const noexcept;

    std::string to_string() const;
    // compatibility: return an int corresponding to Dictionary::TYPE for tests that call .type() on parse_json result
    int type() const;
    std::string dump(int indent = 4, bool compact = true) const;
    // forward map-like helpers when Value holds a dict
    std::vector<std::string> keys() const;
    bool has(const key_type &k) const;
    int count(const key_type &k) const;
    bool contains(const key_type &k) const;
    // forward merge/override when value is dict
    Dictionary overrideEntries(const Dictionary& cfg) const;
    Dictionary merge(const Dictionary& cfg) const;
    // also accept Values when Value holds dictionaries
    Dictionary overrideEntries(const Value& cfg) const;
    Dictionary merge(const Value& cfg) const;
};

struct Dictionary {
    using list_t = std::vector<Dictionary>;
    using key_type = std::string;
    using value_type = Value;
    using map_type = std::map<key_type, value_type>;

    map_type data;
    std::optional<Value> scalar;

    // Keep enum names that tests expect (unscoped enum for direct use as Dictionary::Object etc.)
    enum TYPE { Object, Boolean, String, Integer, Double, IntArray, DoubleArray, StringArray, BoolArray, ObjectArray, Null };

    Dictionary() = default;
    // Explicitly declare copy/move constructors and move assignment to avoid
    // deprecation warnings about implicitly-declared copy constructor when a
    // user-defined copy-assignment operator exists.
    Dictionary(const Dictionary& other) = default;
    Dictionary(Dictionary&& other) noexcept = default;
    Dictionary& operator=(Dictionary&& other) noexcept = default;
    static Value null() { return Value(); }

    Dictionary& operator=(const Dictionary& d) { data = d.data; scalar = d.scalar; return *this; }
    Dictionary& operator=(const std::string& s) { data.clear(); scalar = Value(s); return *this; }
    Dictionary& operator=(const char* s) { return operator=(std::string(s)); }
    Dictionary& operator=(int64_t n) { data.clear(); scalar = Value(n); return *this; }
    Dictionary& operator=(int n) { return operator=(int64_t(n)); }
    Dictionary& operator=(double x) { data.clear(); scalar = Value(x); return *this; }
    Dictionary& operator=(const bool& b) { data.clear(); scalar = Value(b); return *this; }
    Dictionary& operator=(const std::vector<int>& v) { Value::list_t L; for (auto x : v) L.emplace_back(int64_t(x)); data.clear(); scalar = Value(std::move(L)); return *this; }
    Dictionary& operator=(const std::vector<bool>& v) { Value::list_t L; for (bool x : v) L.emplace_back(x); data.clear(); scalar = Value(std::move(L)); return *this; }
    Dictionary& operator=(const std::vector<double>& v) { Value::list_t L; for (auto x : v) L.emplace_back(x); data.clear(); scalar = Value(std::move(L)); return *this; }
    Dictionary& operator=(const std::vector<std::string>& v) { Value::list_t L; for (auto const &x : v) L.emplace_back(x); data.clear(); scalar = Value(std::move(L)); return *this; }
    Dictionary& operator=(const std::vector<Dictionary>& v) { data.clear(); // store as scalar list of Values constructed from dictionaries
        Value::list_t L; L.reserve(v.size()); for (auto const &d: v) L.emplace_back(Value(d)); scalar = Value(std::move(L)); return *this; }
    Dictionary& operator=(std::vector<Dictionary>&& v) { data.clear(); Value::list_t L; L.reserve(v.size()); for (auto &d: v) L.emplace_back(Value(std::move(d))); scalar = Value(std::move(L)); return *this; }
    Dictionary& operator=(Value&& obj) { data.clear(); scalar = std::move(obj); return *this; }
    Dictionary& operator=(const Value& obj) { Value tmp = obj; data.clear(); scalar = std::move(tmp); return *this; }

    bool operator==(const Dictionary& rhs) const { return data == rhs.data && scalar == rhs.scalar; }
    bool operator!=(const Dictionary& rhs) const { return not (*this == rhs); }

    bool isTrue(const key_type& key) const {
        auto it = data.find(key);
        if (it == data.end()) return false;
        return it->second.isBool() && it->second.asBool();
    }
    int count(const key_type& key) const {
        if (data.find(key) != data.end()) return 1;
        if (scalar && scalar->isDict()) return (*scalar->asDict()).count(key);
        return 0;
    }
    bool has(const key_type& key) const noexcept { return count(key) == 1; }
    bool contains(const key_type& k) const noexcept { return has(k); }
    int size() const noexcept {
        if (not data.empty()) return data.size();
        if (scalar) {
            if (scalar->isDict()) return scalar->asDict()->size();
            if (scalar->isList()) return scalar->asList().size();
        }
        return 0;
    }
    bool empty() const noexcept { return data.empty(); }
    bool erase(const key_type& k) { return data.erase(k) > 0; }
    void clear() noexcept { data.clear(); scalar.reset(); }

    TYPE type() const;

    value_type& operator[](int index) {
        if(scalar->isList()) {
            return (*scalar)[index];
        }
        throw std::logic_error("Not a list");
    }
    const value_type& operator[](int index) const {
        if(scalar->isList()) {
            return (*scalar)[index];
        }
        throw std::logic_error("Not a list");
    }
    value_type& at(int index) {
        if(scalar->isList()) {
            return scalar->at(index);
        }
        throw std::logic_error("Not a list");
    }
    const value_type at(int index) const {
        if(scalar->isList()) {
            return scalar->at(index);
        }
        throw std::logic_error("Not a list");
    }
    value_type& operator[](const key_type& k) { return data[k]; }
    const value_type& at(const key_type& k) const {
        auto it = data.find(k);
        if (it != data.end()) return it->second;
        if (scalar && scalar->isDict()) {
            const auto &inner = *scalar->asDict();
            return inner.at(k);
        }
        // If there's exactly one top-level key, suggest it (tests expect this behavior)
        if (data.size() == 1) {
            auto only = data.begin()->first;
            throw std::out_of_range(std::string("\"") + only + std::string("\""));
        }
        throw std::out_of_range("key not found");
    }
    value_type& at(const key_type& k) {
        auto it = data.find(k);
        if (it != data.end()) return it->second;
        if (scalar && scalar->isDict()) {
            auto &inner = *scalar->asDict();
            return inner.at(k);
        }
        if (data.size() == 1) {
            auto only = data.begin()->first;
            throw std::out_of_range(std::string("\"") + only + std::string("\""));
        }
        throw std::out_of_range("key not found");
    }

    std::vector<key_type> keys() const { std::vector<key_type> out; out.reserve(data.size()); for (auto const &p: data) out.push_back(p.first); return out; }
    std::vector<value_type> values() const { std::vector<value_type> out; out.reserve(data.size()); for (auto const &p: data) out.push_back(p.second); return out; }
    std::vector<std::pair<key_type, value_type>> items() const { std::vector<std::pair<key_type, value_type>> out; out.reserve(data.size()); for (auto const &p: data) out.push_back(p); return out; }

    // scalar accessors - keep old names for tests
    std::string asString() const { if (scalar && scalar->isString()) return scalar->asString(); throw std::runtime_error("not a string"); }
    int asInt() const { if (scalar && scalar->isInt()) return static_cast<int>(scalar->asInt()); throw std::runtime_error("not an int"); }
    double asDouble() const { if (scalar && scalar->isDouble()) return scalar->asDouble(); throw std::runtime_error("not a double"); }
    bool asBool() const { if (scalar && scalar->isBool()) return scalar->asBool(); throw std::runtime_error("not a bool"); }

    std::vector<int> asInts() const;
    std::vector<double> asDoubles() const;
    std::vector<std::string> asStrings() const;
    std::vector<bool> asBools() const;
    std::vector<Dictionary> asObjects() const;

    bool isValueObject() const { return scalar.has_value() and not scalar->isList() and not scalar->isDict(); }
    bool isArrayObject() const { return scalar.has_value() && scalar->isList(); }
    bool isMappedObject() const { return !data.empty(); }

    std::string dump(int indent = 4, bool compact = true) const;

    Dictionary overrideEntries(const Dictionary& config) const;
    Dictionary merge(const Dictionary& config) const;
    // convenience overloads that accept Value
    Dictionary overrideEntries(const Value& cfg) const;
    Dictionary merge(const Value& cfg) const;

    Dictionary(std::initializer_list<std::pair<const key_type, value_type>> init)
      : data(init) {}

      std::string to_string() const {
          if (isValueObject()) return scalar->to_string();
          return dump();
      }
};



inline Value::Value(const Dictionary& d) : v(std::make_shared<Dictionary>(d)) {}
inline Value::Value(Dictionary&& d) : v(std::make_shared<Dictionary>(std::move(d))) {}

inline Value::Value(std::initializer_list<std::pair<const std::string, Value>> init) {
    Dictionary d(init);
    v = std::make_shared<Dictionary>(std::move(d));
}

// define vector<Dictionary> constructors now that Dictionary is complete
inline Value::Value(const std::vector<Dictionary>& v_) {
    list_t L; L.reserve(v_.size());
    for (auto const &d: v_) L.emplace_back(Value(d));
    v = std::move(L);
}
inline Value::Value(std::vector<Dictionary>&& v_) {
    list_t L; L.reserve(v_.size());
    for (auto &d: v_) L.emplace_back(Value(std::move(d)));
    v = std::move(L);
}

inline std::string Value::to_string() const {
    if (isNull()) return "null";
    if (isInt()) return std::to_string(asInt());
    if (isDouble()) { std::ostringstream ss; ss << asDouble(); return ss.str(); }
    if (isBool()) return asBool() ? "true" : "false";
    if (isString()) { std::ostringstream ss; ss << '"' << asString() << '"'; return ss.str(); }
    if (isList()) {
        const auto &L = asList(); std::ostringstream ss; ss << '[';
        for (size_t i = 0; i < L.size(); ++i) {
            ss << L[i].dump();
            if (i + 1 < L.size()) ss << ',';
        }
        ss << ']';
        return ss.str();
    }
    if (isDict()) {
        const auto &D = asDict(); return D ? D->dump() : std::string("null");
    }
    return "<unknown>";
}


inline std::vector<int> Value::asInts() const {
    if (isList()) { std::vector<int> out; for (auto const &e: asList()) out.push_back(e.isInt()?static_cast<int>(e.asInt()):0); return out; }
    if (isInt()) return std::vector<int>{static_cast<int>(asInt())};
    throw std::runtime_error("not an int list");
}
inline std::vector<double> Value::asDoubles() const {
    if (isList()) { std::vector<double> out; for (auto const &e: asList()) out.push_back(e.isDouble()?e.asDouble(): (e.isInt()?static_cast<double>(e.asInt()):0.0)); return out; }
    if (isDouble()) return std::vector<double>{asDouble()};
    if (isInt()) return std::vector<double>{static_cast<double>(asInt())};
    throw std::runtime_error("not a double list");
}
inline std::vector<std::string> Value::asStrings() const {
    if (isList()) { std::vector<std::string> out; for (auto const &e: asList()) out.push_back(e.isString()?e.asString():std::string()); return out; }
    if (isString()) return std::vector<std::string>{asString()};
    throw std::runtime_error("not a string list");
}
inline std::vector<bool> Value::asBools() const {
    if (isList()) { std::vector<bool> out; for (auto const &e: asList()) out.push_back(e.isBool()?e.asBool():false); return out; }
    if (isBool()) return std::vector<bool>{asBool()};
    throw std::runtime_error("not a bool list");
}
inline std::vector<Dictionary> Value::asObjects() const {
    if (isList()) { std::vector<Dictionary> out; for (auto const &e: asList()) if (e.isDict()) out.push_back(*e.asDict()); return out; }
    if (isDict()) return std::vector<Dictionary>{*asDict()};
    throw std::runtime_error("not an object list");
}

inline Value& Value::operator[](size_t idx) {
    if (!isList()) { list_t L; v = std::move(L); }
    auto &L = std::get<list_t>(v);
    if (idx >= L.size()) L.resize(idx+1);
    return L[idx];
}
inline const Value& Value::operator[](size_t idx) const {
    if (!isList()) throw std::out_of_range("not a list");
    const auto &L = std::get<list_t>(v);
    if (idx >= L.size()) throw std::out_of_range("index out of range");
    return L[idx];
}
inline Value& Value::operator[](const key_type& k) {
    if (!isDict()) { dict_ptr p = std::make_shared<Dictionary>(); v = p; }
    auto &p = *std::get<dict_ptr>(v);
    return p[k];
}

inline const Value& Value::at(size_t idx) const {
    if (isList()) { const auto &L = asList(); if (idx >= L.size()) throw std::out_of_range("index out of range"); return L[idx]; }
    throw std::out_of_range("not an array");
}
inline Value& Value::at(size_t idx) {
    if (isList()) { auto &L = std::get<list_t>(v); if (idx >= L.size()) throw std::out_of_range("index out of range"); return L[idx]; }
    throw std::out_of_range("not an array");
}
inline const Value& Value::at(const key_type& k) const {
    if (isDict()) { const auto &p = *asDict(); return p.at(k); }
    throw std::out_of_range("not an object");
}
inline Value& Value::at(const key_type& k) {
    if (isDict()) { auto &p = *std::get<dict_ptr>(v); return p.at(k); }
    throw std::out_of_range("not an object");
}
inline int Value::size() const noexcept {
    if (isList()) return asList().size();
    if (isDict()) { const auto &p = asDict(); return p ? p->size() : 0; }
    return 0;
}


inline std::vector<int> Dictionary::asInts() const { if (scalar && scalar->isList()) { std::vector<int> out; for (auto const &e: scalar->asList()) out.push_back(e.isInt()?static_cast<int>(e.asInt()):0); return out; } throw std::runtime_error("not an int list"); }
inline std::vector<double> Dictionary::asDoubles() const { if (scalar && scalar->isList()) { std::vector<double> out; for (auto const &e: scalar->asList()) out.push_back(e.isDouble()?e.asDouble(): (e.isInt()?static_cast<double>(e.asInt()):0.0)); return out; } throw std::runtime_error("not a double list"); }
inline std::vector<std::string> Dictionary::asStrings() const { if (scalar && scalar->isList()) { std::vector<std::string> out; for (auto const &e: scalar->asList()) out.push_back(e.isString()?e.asString():std::string()); return out; } throw std::runtime_error("not a string list"); }
inline std::vector<bool> Dictionary::asBools() const { if (scalar && scalar->isList()) { std::vector<bool> out; for (auto const &e: scalar->asList()) out.push_back(e.isBool()?e.asBool():false); return out; } throw std::runtime_error("not a bool list"); }
inline std::vector<Dictionary> Dictionary::asObjects() const { if (scalar && scalar->isList()) { std::vector<Dictionary> out; for (auto const &e: scalar->asList()) if (e.isDict()) out.push_back(*e.asDict()); return out; } throw std::runtime_error("not an object list"); }

inline std::string Dictionary::dump(int indent, bool compact) const {
    // If this Dictionary is a scalar-only wrapper, print the scalar raw
    if (data.empty() && scalar) {
        return scalar->dump(indent, compact);
    }

    // compact == true and indent == 0 produces a single-line compact JSON
    if (indent == 0) {
        std::ostringstream ss;
        ss << '{';
        bool first = true;
        for (auto const &p: data) {
            if (!first) ss << ',';
            first = false;
            ss << '"' << p.first << '"' << ':' << p.second.dump(0, compact);
        }
        ss << '}';
        return ss.str();
    }

    // (list-type detection moved to a shared helper)

    // produce a one-line pretty-compact representation (spaces after colons and commas)
    std::function<std::string(const Dictionary&)> make_pretty_compact;
    make_pretty_compact = [&](const Dictionary &d) -> std::string {
        std::ostringstream ss;
        ss << "{ ";
        bool first = true;
        for (auto const &p: d.items()) {
            if (!first) ss << ", ";
            first = false;
            ss << '"' << p.first << '"' << ": ";
            const Value &val = p.second;
            if (val.isDict()) {
                ss << make_pretty_compact(*val.asDict());
            } else if (val.isList()) {
                const auto &L = val.asList();
                ss << '[';
                for (size_t i = 0; i < L.size(); ++i) {
                    if (i) ss << ", ";
                    if (L[i].isDict()) ss << make_pretty_compact(*L[i].asDict());
                    else if (L[i].isList()) ss << L[i].dump(0, compact);
                    else ss << L[i].to_string();
                }
                ss << ']';
            } else {
                ss << val.to_string();
            }
        }
        ss << " }";
        return ss.str();
    };

    bool force_expand = false;
    if (compact) {
        std::string one = make_pretty_compact(*this);
        if (one.size() > 80) force_expand = true;
    }

    std::ostringstream out;

    // Forward declarations of small helpers used by the pretty printer
    std::function<void(const Value&, int)> dumpValue;
    std::function<void(const Dictionary&, int)> dumpObject;

    dumpValue = [&](const Value &val, int level) {
        if (val.isNull()) { out << "null"; return; }
        if (val.isInt()) { out << val.asInt(); return; }
        if (val.isDouble()) { std::ostringstream ss; ss << val.asDouble(); out << ss.str(); return; }
        if (val.isBool()) { out << (val.asBool() ? "true" : "false"); return; }
        if (val.isString()) { out << '"' << val.asString() << '"'; return; }
        if (val.isList()) {
            const auto &L = val.asList();
            if (L.empty()) { out << "[]"; return; }
            if (compact) {
                out << '[';
                for (size_t i = 0; i < L.size(); ++i) {
                    out << L[i].dump(0, compact);
                    if (i + 1 < L.size()) out << ", ";
                }
                out << ']';
                return;
            }
            out << "[\n";
            for (size_t i=0;i<L.size();++i) {
                out << std::string(static_cast<size_t>(level+indent), ' ');
                dumpValue(L[i], level+indent);
                if (i+1 < L.size()) out << ",\n"; else out << "\n";
            }
            out << std::string(static_cast<size_t>(level), ' ') << "]";
            return;
        }
        if (val.isDict()) { dumpObject(*val.asDict(), level); return; }
        out << "null";
    };

    dumpObject = [&](const Dictionary &d, int level) {
        auto items = d.items();
        if (items.empty()) { out << "{}"; return; }
        if (compact && !force_expand) {
            std::string one = make_pretty_compact(d);
            if (one.size() <= 80) { out << one; return; }
        }
        out << "{\n";
        for (size_t i=0;i<items.size();++i) {
            out << std::string(static_cast<size_t>(level+indent), ' ') << '"' << items[i].first << '"' << ": ";
            dumpValue(items[i].second, level+indent);
            if (i+1 < items.size()) out << ",\n"; else out << "\n";
        }
    out << std::string(static_cast<size_t>(level), ' ') << "}";
    };

    dumpObject(*this, 0);
    return out.str();
}


inline Dictionary Dictionary::overrideEntries(const Dictionary& config) const {
    Dictionary out = *this;
    for (auto const &p: config.data) {
        auto it = out.data.find(p.first);
        if (it != out.data.end() && it->second.isDict() && p.second.isDict()) {
            Dictionary merged = *it->second.asDict();
            merged = merged.overrideEntries(*p.second.asDict());
            out.data[p.first] = Value(merged);
        } else {
            out.data[p.first] = p.second;
        }
    }
    if (config.scalar) out.scalar = config.scalar;
    return out;
}

inline Dictionary Dictionary::merge(const Dictionary& config) const {
    Dictionary out = *this;
    for (auto const &p: config.data) {
        auto it = out.data.find(p.first);
        if (it != out.data.end() && it->second.isDict() && p.second.isDict()) {
            Dictionary merged = *it->second.asDict();
            merged = merged.merge(*p.second.asDict());
            out.data[p.first] = Value(merged);
        } else {
            out.data[p.first] = p.second;
        }
    }
    if (config.scalar) out.scalar = config.scalar;
    return out;
}

inline Dictionary Dictionary::overrideEntries(const Value& cfg) const { if (cfg.isDict()) return overrideEntries(*cfg.asDict()); throw std::runtime_error("not an object"); }
inline Dictionary Dictionary::merge(const Value& cfg) const { if (cfg.isDict()) return merge(*cfg.asDict()); throw std::runtime_error("not an object"); }

inline std::ostream& operator<<(std::ostream& os, const Dictionary& d) {
    os << d.dump();
    return os;
}

} // namespace ps


namespace ps {

// Helper: detect homogeneous list type - used by Dictionary::type and Value::type
inline Dictionary::TYPE detect_list_type(const Value::list_t &L) {
    if (L.empty()) return Dictionary::ObjectArray;
    bool allInt = true, allDouble = true, allString = true, allBool = true, allObject = true;
    for (auto const &e: L) {
        allInt = allInt and e.isInt();
        allDouble = allDouble and (e.isDouble() or e.isInt());
        allString = allString and e.isString();
        allBool = allBool and e.isBool();
        allObject = allObject and e.isDict();
    }
    if (allInt) return Dictionary::IntArray;
    if (allDouble) return Dictionary::DoubleArray;
    if (allString) return Dictionary::StringArray;
    if (allBool) return Dictionary::BoolArray;
    if (allObject) return Dictionary::ObjectArray;
    return Dictionary::ObjectArray;
}

inline int Value::type() const {
    if (isDict()) {
        const auto &p = asDict();
        return p ? static_cast<int>(p->type()) : static_cast<int>(Dictionary::Null);
    }
    if (isList()) {
        const auto &L = asList();
        if (L.empty()) return static_cast<int>(Dictionary::ObjectArray);
        // defer to Dictionary::type logic for list typing
        Dictionary::TYPE t = detect_list_type(L);
        return static_cast<int>(t);
    }
    if (isString()) return static_cast<int>(Dictionary::String);
    if (isInt()) return static_cast<int>(Dictionary::Integer);
    if (isDouble()) return static_cast<int>(Dictionary::Double);
    if (isBool()) return static_cast<int>(Dictionary::Boolean);
    return static_cast<int>(Dictionary::Null);
}

inline Dictionary::TYPE Dictionary::type() const {
    if (!data.empty()) return TYPE::Object;
    if (scalar.has_value()) {
        if (scalar->isList()) {
            const auto &L = scalar->asList();
            return detect_list_type(L);
        }
        if (scalar->isDict()) return TYPE::Object;
        if (scalar->isString()) return TYPE::String;
        if (scalar->isInt()) return TYPE::Integer;
        if (scalar->isDouble()) return TYPE::Double;
        if (scalar->isBool()) return TYPE::Boolean;
        return TYPE::Null;
    }
    return TYPE::Object;
}

inline std::string Value::dump(int indent, bool compact) const { if (isDict()) { const auto &p = asDict(); return p ? p->dump(indent, compact) : std::string("null"); } return to_string(); }

inline std::vector<std::string> Value::keys() const { if (isDict()) { const auto &p = asDict(); return p ? p->keys() : std::vector<std::string>{}; } throw std::runtime_error("not an object"); }
inline bool Value::has(const key_type &k) const { if (isDict()) { const auto &p = asDict(); return p ? p->has(k) : false; } return false; }
inline int Value::count(const key_type &k) const { if (isDict()) { const auto &p = asDict(); return p ? p->count(k) : 0; } return 0; }
inline bool Value::contains(const key_type &k) const { return has(k); }

inline Dictionary Value::overrideEntries(const Dictionary& cfg) const { if (isDict()) { const auto &p = asDict(); return p ? p->overrideEntries(cfg) : Dictionary(); } throw std::runtime_error("not an object"); }
inline Dictionary Value::merge(const Dictionary& cfg) const { if (isDict()) { const auto &p = asDict(); return p ? p->merge(cfg) : Dictionary(); } throw std::runtime_error("not an object"); }

inline Dictionary Value::overrideEntries(const Value& cfg) const { if (cfg.isDict()) return overrideEntries(*cfg.asDict()); throw std::runtime_error("not an object"); }
inline Dictionary Value::merge(const Value& cfg) const { if (cfg.isDict()) return merge(*cfg.asDict()); throw std::runtime_error("not an object"); }

} // namespace ps
