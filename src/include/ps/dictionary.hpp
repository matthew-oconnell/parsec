// ps::Dictionary - a lightweight Python-like dictionary for C++
#pragma once

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <memory>
#include <stdexcept>
#include <sstream>

namespace ps {

struct Dictionary; // forward

struct Value {
    using list_t = std::vector<Value>;
    using dict_ptr = std::shared_ptr<Dictionary>;

    std::variant<std::monostate, int64_t, double, bool, std::string, list_t, dict_ptr> v;

    Value() = default;

    // Construction helpers
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

    bool operator==(const Value& o) const noexcept {
        return v == o.v;
    }
    std::string to_string() const;
};

struct Dictionary {
    using key_type = std::string;
    using value_type = Value;
    using map_type = std::unordered_map<key_type, value_type>;

    map_type data;

    Dictionary() = default;

    Dictionary(std::initializer_list<std::pair<const key_type, value_type>> init)
      : data(init) {}

    bool contains(const key_type& k) const noexcept {
        return data.find(k) != data.end();
    }

    value_type& operator[](const key_type& k) {
        return data[k];
    }

    const value_type& at(const key_type& k) const {
        auto it = data.find(k);
        if (it == data.end()) throw std::out_of_range("key not found");
        return it->second;
    }

    size_t size() const noexcept { return data.size(); }
    bool empty() const noexcept { return data.empty(); }

    bool erase(const key_type& k) { return data.erase(k) > 0; }

    void clear() noexcept { data.clear(); }

    std::vector<key_type> keys() const {
        std::vector<key_type> out; out.reserve(data.size());
        for (auto const& p : data) out.push_back(p.first);
        return out;
    }

    std::vector<value_type> values() const {
        std::vector<value_type> out; out.reserve(data.size());
        for (auto const& p : data) out.push_back(p.second);
        return out;
    }

    std::vector<std::pair<key_type, value_type>> items() const {
        std::vector<std::pair<key_type, value_type>> out; out.reserve(data.size());
        for (auto const& p : data) out.push_back(p);
        return out;
    }
};

// Value constructors that depend on Dictionary
inline Value::Value(const Dictionary& d) : v(std::make_shared<Dictionary>(d)) {}
inline Value::Value(Dictionary&& d) : v(std::make_shared<Dictionary>(std::move(d))) {}

} // namespace ps

// Implement to_string after Dictionary is defined
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
