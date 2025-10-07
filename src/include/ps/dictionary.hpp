// ps::Dictionary - a lightweight Python-like dictionary for C++
#pragma once

#include <string>
#include <unordered_map>
// ps::Dictionary - a lightweight Python-like dictionary for C++
#pragma once

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <memory>
#include <stdexcept>
// ps::Dictionary - a lightweight Python-like dictionary for C++
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <optional>

namespace ps {

struct Dictionary; // forward

struct Value {
    using list_t = std::vector<Value>;
    using dict_ptr = std::shared_ptr<Dictionary>;

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

    bool is_null() const noexcept { return std::holds_alternative<std::monostate>(v); }
    bool is_int() const noexcept { return std::holds_alternative<int64_t>(v); }
    bool is_double() const noexcept { return std::holds_alternative<double>(v); }
    bool is_bool() const noexcept { return std::holds_alternative<bool>(v); }
    bool is_string() const noexcept { return std::holds_alternative<std::string>(v); }
    bool is_list() const noexcept { return std::holds_alternative<list_t>(v); }
    bool is_dict() const noexcept { return std::holds_alternative<dict_ptr>(v); }

    int64_t as_int() const { return std::get<int64_t>(v); }
    double as_double() const { return std::get<double>(v); }
    bool as_bool() const { return std::get<bool>(v); }
    const std::string& as_string() const { return std::get<std::string>(v); }
    const list_t& as_list() const { return std::get<list_t>(v); }
    const dict_ptr& as_dict() const { return std::get<dict_ptr>(v); }

    bool operator==(const Value& o) const noexcept { return v == o.v; }
    std::string to_string() const;
};

struct Dictionary {
    using key_type = std::string;
    using value_type = Value;
    using map_type = std::unordered_map<key_type, value_type>;

    map_type data;
    std::optional<value_type> scalar;

    enum class TYPE { Object, MappedObject, ArrayObject, ValueObject, Null };

    Dictionary() = default;
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
    Dictionary& operator=(Value&& obj) { data.clear(); scalar = std::move(obj); return *this; }

    bool operator==(const Dictionary& rhs) const { return data == rhs.data && scalar == rhs.scalar; }
    bool operator!=(const Dictionary& rhs) const { return !(*this == rhs); }

    bool isTrue(const key_type& key) const {
        auto it = data.find(key);
        if (it == data.end()) return false;
        return it->second.is_bool() && it->second.as_bool();
    }
    int count(const key_type& key) const { return data.find(key) != data.end() ? 1 : 0; }
    bool has(const key_type& key) const noexcept { return count(key) == 1; }
    bool contains(const key_type& k) const noexcept { return data.find(k) != data.end(); }
    size_t size() const noexcept { return data.size(); }
    bool empty() const noexcept { return data.empty(); }
    bool erase(const key_type& k) { return data.erase(k) > 0; }
    void clear() noexcept { data.clear(); scalar.reset(); }

    TYPE type() const {
        if (scalar.has_value()) {
            if (scalar->is_list()) return TYPE::ArrayObject;
            if (scalar->is_dict()) return TYPE::MappedObject;
            return TYPE::ValueObject;
        }
        if (!data.empty()) return TYPE::MappedObject;
        return TYPE::Null;
    }

    value_type& operator[](const key_type& k) { return data[k]; }
    const value_type& at(const key_type& k) const {
        auto it = data.find(k);
        if (it == data.end()) throw std::out_of_range("key not found");
        return it->second;
    }
    value_type& at(const key_type& k) {
        auto it = data.find(k);
        if (it == data.end()) throw std::out_of_range("key not found");
        return it->second;
    }

    std::vector<key_type> keys() const { std::vector<key_type> out; out.reserve(data.size()); for (auto const &p: data) out.push_back(p.first); return out; }
    std::vector<value_type> values() const { std::vector<value_type> out; out.reserve(data.size()); for (auto const &p: data) out.push_back(p.second); return out; }
    std::vector<std::pair<key_type, value_type>> items() const { std::vector<std::pair<key_type, value_type>> out; out.reserve(data.size()); for (auto const &p: data) out.push_back(p); return out; }

    std::string asString() const { if (scalar && scalar->is_string()) return scalar->as_string(); throw std::runtime_error("not a string"); }
    int asInt() const { if (scalar && scalar->is_int()) return static_cast<int>(scalar->as_int()); throw std::runtime_error("not an int"); }
    double asDouble() const { if (scalar && scalar->is_double()) return scalar->as_double(); throw std::runtime_error("not a double"); }
    bool asBool() const { if (scalar && scalar->is_bool()) return scalar->as_bool(); throw std::runtime_error("not a bool"); }
    std::vector<int> asInts() const { if (scalar && scalar->is_list()) { std::vector<int> out; for (auto const &e: scalar->as_list()) out.push_back(e.is_int()?static_cast<int>(e.as_int()):0); return out; } throw std::runtime_error("not an int list"); }
    std::vector<double> asDoubles() const { if (scalar && scalar->is_list()) { std::vector<double> out; for (auto const &e: scalar->as_list()) out.push_back(e.is_double()?e.as_double(): (e.is_int()?static_cast<double>(e.as_int()):0.0)); return out; } throw std::runtime_error("not a double list"); }
    std::vector<std::string> asStrings() const { if (scalar && scalar->is_list()) { std::vector<std::string> out; for (auto const &e: scalar->as_list()) out.push_back(e.is_string()?e.as_string():std::string()); return out; } throw std::runtime_error("not a string list"); }
    std::vector<bool> asBools() const { if (scalar && scalar->is_list()) { std::vector<bool> out; for (auto const &e: scalar->as_list()) out.push_back(e.is_bool()?e.as_bool():false); return out; } throw std::runtime_error("not a bool list"); }
    std::vector<Dictionary> asObjects() const { if (scalar && scalar->is_list()) { std::vector<Dictionary> out; for (auto const &e: scalar->as_list()) if (e.is_dict()) out.push_back(*e.as_dict()); return out; } throw std::runtime_error("not an object list"); }

    bool isValueObject() const { return scalar.has_value() && !scalar->is_list() && !scalar->is_dict(); }
    bool isMappedObject() const { return !data.empty(); }
    bool isArrayObject() const { return scalar.has_value() && scalar->is_list(); }

    std::string dump() const {
        std::ostringstream ss;
        ss << "{";
        bool first = true;
        for (auto const &p: data) {
            if (!first) ss << ", ";
            first = false;
            ss << '"' << p.first << '"' << ": " << p.second.to_string();
        }
        if (scalar) {
            if (!data.empty()) ss << ", ";
            ss << scalar->to_string();
        }
        ss << "}";
        return ss.str();
    }

    Dictionary overrideEntries(const Dictionary& config) const {
        Dictionary out = *this;
        for (auto const &p: config.data) out.data[p.first] = p.second;
        if (config.scalar) out.scalar = config.scalar;
        return out;
    }

    Dictionary merge(const Dictionary& config) const {
        Dictionary out = *this;
        for (auto const &p: config.data) {
            auto it = out.data.find(p.first);
            if (it != out.data.end() && it->second.is_dict() && p.second.is_dict()) {
                Dictionary merged = *it->second.as_dict();
                merged = merged.merge(*p.second.as_dict());
                out.data[p.first] = Value(merged);
            } else {
                out.data[p.first] = p.second;
            }
        }
        if (config.scalar) out.scalar = config.scalar;
        return out;
    }

    Dictionary(std::initializer_list<std::pair<const key_type, value_type>> init)
      : data(init) {}
};

inline Value::Value(const Dictionary& d) : v(std::make_shared<Dictionary>(d)) {}
inline Value::Value(Dictionary&& d) : v(std::make_shared<Dictionary>(std::move(d))) {}

} // namespace ps

namespace ps {
inline std::string Value::to_string() const {
    if (is_null()) return "null";
    if (is_int()) return std::to_string(as_int());
    if (is_double()) { std::ostringstream ss; ss << as_double(); return ss.str(); }
    if (is_bool()) return as_bool() ? "true" : "false";
    if (is_string()) { std::ostringstream ss; ss << '"' << as_string() << '"'; return ss.str(); }
    if (is_list()) { const auto &L = as_list(); std::ostringstream ss; ss << "[array, " << L.size() << " items]"; return ss.str(); }
    if (is_dict()) { const auto &D = as_dict(); size_t n = D ? D->size() : 0; std::ostringstream ss; ss << "{object, " << n << " keys}"; return ss.str(); }
    return "<unknown>";
}
} // namespace ps
