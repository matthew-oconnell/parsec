#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <initializer_list>
#include <map>
#include <variant>
#include <vector>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <ostream>
#include <optional>
#include <functional>
#include <type_traits>

namespace ps {
struct Dictionary {
    enum TYPE {
        Object,
        Boolean,
        String,
        Integer,
        Double,
        IntArray,
        DoubleArray,
        StringArray,
        BoolArray,
        ObjectArray,
        Null
    };

    TYPE my_type = Object;
    bool m_bool;
    double m_double;
    int64_t m_int;
    std::string m_string;
    std::unique_ptr<Dictionary> m_object;

    std::vector<Dictionary> m_object_array;
    std::map<std::string, Dictionary> m_object_map;

    Dictionary() { my_type = TYPE::Object; }

    explicit Dictionary(const std::string& s) {
        my_type = TYPE::String;
        m_string = s;
    }

    explicit Dictionary(const char* s) { *this = std::string(s); }

    explicit Dictionary(int64_t n) {
        my_type = TYPE::Integer;
        m_int = n;
    }

    explicit Dictionary(int n) { *this = int64_t(n); }

    explicit Dictionary(double x) {
        my_type = TYPE::Double;
        m_double = x;
    }

    explicit Dictionary(bool b) {
        my_type = TYPE::Boolean;
        m_bool = b;
    }

    explicit Dictionary(const std::vector<double>& v) {
        m_object_array.reserve(v.size());
        my_type = TYPE::DoubleArray;
        for (auto d : v) {
            m_object_array.emplace_back(d);
        }
    }

    explicit Dictionary(const std::vector<int>& v) {
        m_object_array.reserve(v.size());
        my_type = TYPE::IntArray;
        for (auto d : v) {
            m_object_array.emplace_back(d);
        }
    }

    explicit Dictionary(const std::vector<std::string>& v) {
        m_object_array.reserve(v.size());
        my_type = TYPE::StringArray;
        for (auto d : v) {
            m_object_array.emplace_back(d);
        }
    }

    explicit Dictionary(const std::vector<bool>& v) {
        m_object_array.reserve(v.size());
        my_type = TYPE::BoolArray;
        for (bool d : v) {
            m_object_array.emplace_back(d);
        }
    }

    explicit Dictionary(const std::vector<Dictionary>& v) {
        m_object_array.reserve(v.size());
        my_type = TYPE::ObjectArray;
        for (auto d : v) {
            m_object_array.emplace_back(d);
        }
    }

    // Construct an object from initializer list of (key, value) pairs
    Dictionary(std::initializer_list<std::pair<std::string, Dictionary> > init) {
        my_type = TYPE::Object;
        for (auto const& p : init) {
            m_object_map.emplace(p.first, p.second);
        }
    }

    // Construct an array from an initializer list of convertible values
    template <typename T,
              typename = std::enable_if_t<!std::is_same<std::decay_t<T>, std::pair<std::string, Dictionary> >::value> >
    explicit Dictionary(std::initializer_list<T> init) {
        my_type = TYPE::ObjectArray;
        m_object_array.reserve(init.size());
        for (auto const& v : init) m_object_array.emplace_back(v);
    }

    Dictionary(const Dictionary& other) {
        my_type = other.my_type;
        switch (my_type) {
            case TYPE::Object:
                m_object_map = other.m_object_map;
                break;
            case TYPE::Boolean:
                m_bool = other.m_bool;
                break;
            case TYPE::Double:
                m_double = other.m_double;
                break;
            case TYPE::Integer:
                m_int = other.m_int;
                break;
            case TYPE::String:
                m_string = other.m_string;
                break;
            case TYPE::ObjectArray:
            case TYPE::IntArray:
            case TYPE::DoubleArray:
            case TYPE::StringArray:
            case TYPE::BoolArray:
                m_object_array = other.m_object_array;
                break;
            case TYPE::Null:
                // nothing to copy
                break;
        }
    }

    Dictionary(Dictionary&& other) noexcept = default;

    Dictionary& operator=(Dictionary&& other) noexcept = default;

    static Dictionary null() {
        Dictionary d;
        d.my_type = TYPE::Null;
        return d;
    }

    Dictionary& operator=(const Dictionary& d) {
        *this = Dictionary(d);
        return *this;
    }

    Dictionary& operator=(const std::string& s) {
        my_type = TYPE::String;
        m_string = s;
        return *this;
    }

    Dictionary& operator=(const char* s) { return operator=(std::string(s)); }

    Dictionary& operator=(int64_t n) {
        m_int = n;
        my_type = TYPE::Integer;
        return *this;
    }

    Dictionary& operator=(int n) { return operator=(int64_t(n)); }

    Dictionary& operator=(double x) {
        m_double = x;
        my_type = TYPE::Double;
        return *this;
    }

    Dictionary& operator=(const bool& b) {
        m_bool = b;
        my_type = TYPE::Boolean;
        return *this;
    }

    Dictionary& operator=(const std::vector<int>& v) {
        std::vector<Dictionary> L;
        for (auto x : v) {
            L.emplace_back();
            L.back() = int64_t(x);
        }
        m_object_array = std::move(L);
        my_type = TYPE::IntArray;
        return *this;
    }

    Dictionary& operator=(const std::vector<bool>& v) {
        std::vector<Dictionary> L;
        for (bool x : v) {
            L.emplace_back();
            L.back() = x;
        }
        m_object_array = std::move(L);
        my_type = TYPE::BoolArray;
        return *this;
    }

    Dictionary& operator=(const std::vector<double>& v) {
        std::vector<Dictionary> L;
        for (auto x : v) {
            L.emplace_back();
            L.back() = x;
        }
        m_object_array = std::move(L);
        my_type = TYPE::DoubleArray;
        return *this;
    }

    Dictionary& operator=(const std::vector<std::string>& v) {
        std::vector<Dictionary> L;
        for (auto const& x : v) {
            L.emplace_back();
            L.back() = x;
        }
        m_object_array = std::move(L);
        my_type = TYPE::StringArray;
        return *this;
    }

    Dictionary& operator=(const std::vector<Dictionary>& v) {
        m_object_array = v;
        my_type = TYPE::ObjectArray;
        return *this;
    }

    Dictionary& operator=(std::vector<Dictionary>&& v) {
        m_object_array = std::move(v);
        my_type = TYPE::ObjectArray;
        return *this;
    }

    bool operator==(const Dictionary& rhs) const {
        if (my_type != rhs.my_type) return false;
        switch (my_type) {
            case TYPE::Boolean:
                return m_bool == rhs.m_bool;
            case TYPE::Double:
                return m_double == rhs.m_double;
            case TYPE::Integer:
                return m_int == rhs.m_int;
            case TYPE::String:
                return m_string == rhs.m_string;
            case TYPE::BoolArray:
            case TYPE::DoubleArray:
            case TYPE::IntArray:
            case TYPE::StringArray:
                return m_object_array == rhs.m_object_array;
            case TYPE::Object:
                return m_object_map == rhs.m_object_map;
            case TYPE::Null:
                return true;
            default:
                break;
        }
        return false;
    }

    bool operator!=(const Dictionary& rhs) const { return not(*this == rhs); }

    bool isTrue(const std::string& key) const {
        if (my_type != TYPE::Object) return false;
        auto it = m_object_map.find(key);
        if (it == m_object_map.end()) return false;
        return it->second.type() == TYPE::Boolean && it->second.asBool();
    }

    int count(const std::string& key) const {
        if (my_type != TYPE::Object) return 0;
        return m_object_map.count(key);
    }

    bool has(const std::string& key) const noexcept { return count(key) == 1; }
    bool contains(const std::string& k) const noexcept { return has(k); }

    int size() const noexcept {
        // I'm not sure what this should mean for scalar types
        switch (my_type) {
            case TYPE::BoolArray:
            case TYPE::DoubleArray:
            case TYPE::IntArray:
            case TYPE::StringArray:
            case TYPE::ObjectArray:
                return m_object_array.size();
            case TYPE::Object:
                return m_object_map.size();
            default:
                return 0;
        }
    }

    bool empty() const noexcept {
        // I'm not sure what this should mean for non-mapped types
        switch (my_type) {
            case TYPE::Object:
                return m_object_map.empty();
            case TYPE::BoolArray:
            case TYPE::DoubleArray:
            case TYPE::IntArray:
            case TYPE::StringArray:
            case TYPE::ObjectArray:
                return m_object_array.empty();
            default:
                return false;
        }
    }

    Dictionary& erase(const std::string& k) {
        if (my_type == TYPE::Object) {
            m_object_map.erase(k);
        }
        return *this;
    }

    void clear() noexcept {
        m_object_map.clear();
        m_object_array.clear();
        m_object.reset();
        my_type = TYPE::Object;
    }

    TYPE type() const { return my_type; }

    std::string typeString() const {
        switch (my_type) {
            case TYPE::Boolean:
                return "Boolean";
            case TYPE::Double:
                return "Double";
            case TYPE::Integer:
                return "Integer";
            case TYPE::String:
                return "String";
            case TYPE::BoolArray:
                return "BoolArray";
            case TYPE::DoubleArray:
                return "DoubleArray";
            case TYPE::IntArray:
                return "IntArray";
            case TYPE::StringArray:
                return "StringArray";
            case TYPE::ObjectArray:
                return "ObjectArray";
            case TYPE::Null:
                return "Null";
            default:
                throw std::logic_error("Not a valid type");
        }
    }

    template <typename DefaultValue>
    Dictionary get(const std::string& key, const DefaultValue& default_value) const {
        if (has(key)) return at(key);
        Dictionary result;
        result = default_value;
        return result;
    }

    Dictionary& operator[](int index) {
        // If this is an object that has never been used as a mapped object,
        // allow converting it to an array on first integer-index access so
        // usage like dict["arr"][0] = 5 works naturally.
        if (my_type == TYPE::Object && m_object_map.empty()) {
            my_type = TYPE::ObjectArray;
            m_object_array.clear();
        }
        if (my_type == TYPE::ObjectArray || my_type == TYPE::IntArray || my_type == TYPE::DoubleArray ||
            my_type == TYPE::StringArray || my_type == TYPE::BoolArray) {
            if (static_cast<size_t>(index) >= m_object_array.size())
                m_object_array.resize(static_cast<size_t>(index) + 1);
            return m_object_array[index];
        }
        throw std::logic_error("Not a list");
    }

    const Dictionary& operator[](int index) const {
        if (my_type == TYPE::ObjectArray || my_type == TYPE::IntArray || my_type == TYPE::DoubleArray ||
            my_type == TYPE::StringArray || my_type == TYPE::BoolArray) {
            return m_object_array[index];
        }
        throw std::logic_error("Not a list");
    }

    Dictionary& operator[](const std::string& k) {
        if (my_type != TYPE::Object) {
            my_type = TYPE::Object;
            m_object_map.clear();
        }
        return m_object_map[k];
    }

    const Dictionary& operator[](const std::string& k) const { return m_object_map.at(k); }

    Dictionary& at(int index) {
        if (my_type == TYPE::ObjectArray || my_type == TYPE::IntArray || my_type == TYPE::DoubleArray ||
            my_type == TYPE::StringArray || my_type == TYPE::BoolArray) {
            return m_object_array.at(index);
        }
        throw std::logic_error("Not a list");
    }

    const Dictionary& at(int index) const {
        if (my_type == TYPE::ObjectArray || my_type == TYPE::IntArray || my_type == TYPE::DoubleArray ||
            my_type == TYPE::StringArray || my_type == TYPE::BoolArray) {
            return m_object_array.at(index);
        }
        throw std::logic_error("Not a list");
    }

    const Dictionary& at(const std::string& k) const {
        auto it = m_object_map.find(k);
        if (it != m_object_map.end()) return it->second;
        std::ostringstream ss;
        bool first = true;
        for (auto const& p : m_object_map) {
            if (!first) ss << ",";
            first = false;
            ss << '"' << p.first << '"';
        }
        throw std::out_of_range(ss.str());
    }

    Dictionary& at(const std::string& k) {
        auto it = m_object_map.find(k);
        if (it != m_object_map.end()) return it->second;
        std::ostringstream ss;
        bool first = true;
        for (auto const& p : m_object_map) {
            if (!first) ss << ",";
            first = false;
            ss << '"' << p.first << '"';
        }
        throw std::out_of_range(ss.str());
    }

    std::vector<std::string> keys() const {
        if (my_type != TYPE::Object) {
            throw std::logic_error("Cannot get keys of non-object type");
        }
        std::vector<std::string> out;
        out.reserve(m_object_map.size());
        for (auto const& p : m_object_map) out.push_back(p.first);
        return out;
    }

    std::vector<Dictionary> values() const {
        if (my_type != TYPE::Object) {
            throw std::logic_error("Cannot get values of non-object type");
        }
        std::vector<Dictionary> out;
        out.reserve(m_object_map.size());
        for (auto const& p : m_object_map) out.push_back(p.second);
        return out;
    }

    std::vector<std::pair<std::string, Dictionary> > items() const {
        if (my_type != TYPE::Object) {
            throw std::logic_error("Cannot get items of non-object type");
        }
        std::vector<std::pair<std::string, Dictionary> > out;
        out.reserve(m_object_map.size());
        for (auto [key, value] : m_object_map) {
            out.push_back({key, value});
        }
        return out;
    }

    std::string asString() const {
        if (my_type == TYPE::String) return m_string;
        if (my_type == TYPE::Integer) return std::to_string(m_int);
        if (my_type == TYPE::Double) {
            std::ostringstream ss;
            ss << m_double;
            return ss.str();
        }
        if (my_type == TYPE::Boolean) return m_bool ? "true" : "false";
        throw std::runtime_error("not a string");
    }

    std::array<double, 3> asPoint() const {
        if (my_type == TYPE::IntArray or my_type == TYPE::DoubleArray) {
            if (m_object_array.size() == 3) {
                std::array<double, 3> out;
                out[0] = m_object_array.at(0).asDouble();
                out[1] = m_object_array.at(1).asDouble();
                out[2] = m_object_array.at(2).asDouble();
                return out;
            }
        }
        throw std::runtime_error("not a 3 element number array for asPoint()");
    }

    bool getBool(const std::string& key) const { return at(key).asBool(); }
    int getInt(const std::string& key) const { return at(key).asInt(); }
    double getDouble(const std::string& key) const { return at(key).asDouble(); }
    std::string getString(const std::string& key) const { return at(key).asString(); }
    std::vector<int> getInts(const std::string& key) const { return at(key).asInts(); }
    std::vector<double> getDoubles(const std::string& key) const { return at(key).asDoubles(); }
    std::vector<std::string> getStrings(const std::string& key) const { return at(key).asStrings(); }
    std::vector<bool> getBools(const std::string& key) const { return at(key).asBools(); }

    int asInt() const {
        if (my_type == TYPE::Integer) return static_cast<int>(m_int);
        if (my_type == TYPE::Double) return static_cast<int>(m_double);
        throw std::runtime_error("not an int");
    }

    double asDouble() const {
        if (my_type == TYPE::Double) return m_double;
        if (my_type == TYPE::Integer) return static_cast<double>(m_int);
        throw std::runtime_error("not a double");
    }

    bool asBool() const {
        if (my_type == TYPE::Boolean) return m_bool;
        throw std::runtime_error("not a bool");
    }

    std::vector<int> asInts() const;

    std::vector<double> asDoubles() const;

    std::vector<std::string> asStrings() const;

    std::vector<bool> asBools() const;

    std::vector<Dictionary> asObjects() const;

    bool isValueObject() const {
        switch (my_type) {
            case TYPE::Object:
            case TYPE::BoolArray:
            case TYPE::DoubleArray:
            case TYPE::IntArray:
            case TYPE::StringArray:
            case TYPE::ObjectArray:
            case TYPE::Null:
                return false;
            default:
                return true;
        }
    }

    bool isArrayObject() const {
        switch (my_type) {
            case TYPE::BoolArray:
            case TYPE::DoubleArray:
            case TYPE::IntArray:
            case TYPE::StringArray:
            case TYPE::ObjectArray:
                return true;
            default:
                return false;
        }
    }

    bool isMappedObject() const {
        if (my_type == TYPE::Object) return true;
        return false;
    }

    // Convenience predicates for legacy API compatibility
    bool isDict() const { return isMappedObject(); }
    bool isList() const { return isArrayObject(); }
    bool isInt() const { return my_type == TYPE::Integer; }
    bool isDouble() const { return my_type == TYPE::Double; }
    bool isString() const { return my_type == TYPE::String; }
    bool isBool() const { return my_type == TYPE::Boolean; }
    bool isNull() const { return my_type == TYPE::Null; }

    // Comparison operators against primitive types for compatibility
    bool operator==(int rhs) const {
        if (my_type == TYPE::Integer) return static_cast<int>(m_int) == rhs;
        if (my_type == TYPE::Double) return static_cast<int>(m_double) == rhs;
        return false;
    }

    bool operator!=(int rhs) const { return not(*this == rhs); }

    bool operator==(double rhs) const {
        if (my_type == TYPE::Double) return m_double == rhs;
        if (my_type == TYPE::Integer) return static_cast<double>(m_int) == rhs;
        return false;
    }

    bool operator!=(double rhs) const { return not(*this == rhs); }

    bool operator==(bool rhs) const {
        if (my_type == TYPE::Boolean) return m_bool == rhs;
        return false;
    }

    bool operator!=(bool rhs) const { return not(*this == rhs); }

    std::string dump(int indent = 4, bool compact = true) const;

    Dictionary overrideEntries(const Dictionary& config) const;

    Dictionary merge(const Dictionary& config) const;

    Dictionary removeCommonEntries(const Dictionary& config) const;

    // convenience overloads that accept Value

    std::string to_string() const {
        switch (my_type) {
            case TYPE::Null:
                return "null";
            case TYPE::Boolean:
                return m_bool ? "true" : "false";
            case TYPE::Integer:
                return std::to_string(m_int);
            case TYPE::Double:
                return std::to_string(m_double);
            default:
                break;
        }
        return dump();
    }
};

inline std::vector<int> Dictionary::asInts() const {
    if (my_type == TYPE::Integer) {
        return std::vector<int>{static_cast<int>(m_int)};
    }
    if (my_type == TYPE::Double) {
        return std::vector<int>{static_cast<int>(m_double)};
    }
    if (my_type == TYPE::IntArray) {
        std::vector<int> out;
        for (auto const& e : m_object_array) out.push_back(e.asInt());
        return out;
    }
    if (my_type == TYPE::DoubleArray) {
        // TODO warn if double isn't approx an int
        std::vector<int> out;
        for (auto const& e : m_object_array) out.push_back(static_cast<int>(e.asDouble()));
        return out;
    }
    if (my_type == TYPE::ObjectArray) {
        std::vector<int> out;
        for (auto const& e : m_object_array) out.push_back(e.asInt());
        return out;
    }
    throw std::runtime_error("not an int list");
}

inline std::vector<double> Dictionary::asDoubles() const {
    if (my_type == TYPE::Double) {
        return std::vector<double>{m_double};
    }
    if (my_type == TYPE::Integer) {
        return std::vector<double>{static_cast<double>(m_int)};
    }
    if (my_type == TYPE::DoubleArray) {
        std::vector<double> out;
        for (auto const& e : m_object_array) out.push_back(e.asDouble());
        return out;
    }
    if (my_type == TYPE::IntArray) {
        std::vector<double> out;
        for (auto const& e : m_object_array) out.push_back(static_cast<double>(e.asInt()));
        return out;
    }
    if (my_type == TYPE::ObjectArray) {
        std::vector<double> out;
        for (auto const& e : m_object_array) out.push_back(e.asDouble());
        return out;
    }
    throw std::runtime_error("not a double list");
}

inline std::vector<std::string> Dictionary::asStrings() const {
    if (my_type == TYPE::String) {
        return std::vector<std::string>{m_string};
    }
    if (my_type == TYPE::StringArray) {
        std::vector<std::string> out;
        for (auto const& e : m_object_array) out.push_back(e.asString());
        return out;
    }
    if (my_type == TYPE::ObjectArray) {
        std::vector<std::string> out;
        for (auto const& e : m_object_array) out.push_back(e.asString());
        return out;
    }
    throw std::runtime_error("not a string list");
}

inline std::vector<bool> Dictionary::asBools() const {
    if (my_type == TYPE::Boolean) {
        return std::vector<bool>{m_bool};
    }
    if (my_type == TYPE::BoolArray) {
        std::vector<bool> out;
        for (auto const& e : m_object_array) out.push_back(e.asBool());
        return out;
    }
    if (my_type == TYPE::ObjectArray) {
        std::vector<bool> out;
        for (auto const& e : m_object_array) out.push_back(e.asBool());
        return out;
    }
    throw std::runtime_error("not a bool list");
}

inline std::vector<Dictionary> Dictionary::asObjects() const {
    if (my_type == TYPE::ObjectArray) {
        return m_object_array;
    }
    if (my_type == TYPE::Object) {
        return std::vector<Dictionary>{*this};
    }
    throw std::runtime_error("not an object list");
}

inline std::string Dictionary::dump(int indent, bool compact) const {
    // If indent == 0 and compact == true we want a "tight" compact form
    // (no spaces after commas or colons) because many tests expect that
    // exact formatting when requesting a zero-indent dump (dump(0)).
    bool tight = (indent == 0 && compact);

    // Produce a compact one-line representation used as a heuristic to decide
    // whether to keep small structures on one line.
    std::function<std::string(const Dictionary&)> make_pretty_compact;
    make_pretty_compact = [&](const Dictionary& d) -> std::string {
        switch (d.my_type) {
            case TYPE::Null:
                return "null";
            case TYPE::Boolean:
                return d.m_bool ? "true" : "false";
            case TYPE::Integer:
                return std::to_string(d.m_int);
            case TYPE::Double: {
                std::ostringstream ss;
                ss << d.m_double;
                return ss.str();
            }
            case TYPE::String:
                return std::string("\"") + d.m_string + std::string("\"");
            case TYPE::IntArray:
            case TYPE::DoubleArray:
            case TYPE::StringArray:
            case TYPE::BoolArray:
            case TYPE::ObjectArray: {
                std::ostringstream ss;
                ss << '[';
                bool first = true;
                for (auto const& el : d.m_object_array) {
                    if (!first) ss << (tight ? "," : ", ");
                    first = false;
                    ss << make_pretty_compact(el);
                }
                ss << ']';
                return ss.str();
            }
            case TYPE::Object: {
                if (d.m_object_map.empty()) return std::string("{}");
                std::ostringstream ss;
                if (tight)
                    ss << '{';
                else
                    ss << "{ ";
                bool first = true;
                for (auto const& p : d.m_object_map) {
                    if (!first) ss << (tight ? "," : ", ");
                    first = false;
                    ss << '"' << p.first << '"' << (tight ? ":" : ": ") << make_pretty_compact(p.second);
                }
                if (tight)
                    ss << '}';
                else
                    ss << " }";
                return ss.str();
            }
        }
        return std::string();
    };

    bool force_expand = false;
    if (compact) {
        std::string one = make_pretty_compact(*this);
        if (one.size() > 80) force_expand = true;
        if (!force_expand && one.size() <= 80) return one;
    }

    std::ostringstream out;

    std::function<void(const Dictionary&, int)> dumpValue;
    std::function<void(const Dictionary&, int)> dumpObject;

    dumpValue = [&](const Dictionary& val, int level) {
        switch (val.my_type) {
            case TYPE::Null:
                out << "null";
                return;
            case TYPE::Boolean:
                out << (val.m_bool ? "true" : "false");
                return;
            case TYPE::Integer:
                out << val.m_int;
                return;
            case TYPE::Double: {
                std::ostringstream ss;
                ss << val.m_double;
                out << ss.str();
                return;
            }
            case TYPE::String:
                out << '"' << val.m_string << '"';
                return;
            case TYPE::IntArray:
            case TYPE::DoubleArray:
            case TYPE::StringArray:
            case TYPE::BoolArray:
            case TYPE::ObjectArray: {
                const auto& L = val.m_object_array;
                if (L.empty()) {
                    out << "[]";
                    return;
                }
                if (compact) {
                    out << '[';
                    for (size_t i = 0; i < L.size(); ++i) {
                        if (i) out << (tight ? "," : ", ");
                        out << make_pretty_compact(L[i]);
                    }
                    out << ']';
                    return;
                }
                out << "[\n";
                for (size_t i = 0; i < L.size(); ++i) {
                    out << std::string(static_cast<size_t>(level + indent), ' ');
                    dumpValue(L[i], level + indent);
                    if (i + 1 < L.size())
                        out << ",\n";
                    else
                        out << "\n";
                }
                out << std::string(static_cast<size_t>(level), ' ') << "]";
                return;
            }
            case TYPE::Object:
                dumpObject(val, level);
                return;
        }
    };

    dumpObject = [&](const Dictionary& d, int level) {
        if (d.m_object_map.empty()) {
            out << "{}";
            return;
        }
        if (compact && !force_expand) {
            std::string one = make_pretty_compact(d);
            if (one.size() <= 80) {
                out << one;
                return;
            }
        }
        out << "{\n";
        auto items = d.items();
        for (size_t i = 0; i < items.size(); ++i) {
            out << std::string(static_cast<size_t>(level + indent), ' ') << '"' << items[i].first << '"' << ": ";
            dumpValue(items[i].second, level + indent);
            if (i + 1 < items.size())
                out << ",\n";
            else
                out << "\n";
        }
        out << std::string(static_cast<size_t>(level), ' ') << "}";
    };

    dumpObject(*this, 0);
    return out.str();

    // // compact == true and indent == 0 produces a single-line compact JSON
    // if (indent == 0) {
    //     std::ostringstream ss;
    //     ss << '{';
    //     bool first = true;
    //     for (auto const& p : data) {
    //         if (!first) ss << ',';
    //         first = false;
    //         ss << '"' << p.first << '"' << ':' << p.second.dump(0, compact);
    //     }
    //     ss << '}';
    //     return ss.str();
    // }

    // // produce a one-line pretty-compact representation (spaces after colons and commas)
    // std::function<std::string(const Dictionary&)> make_pretty_compact;
    // make_pretty_compact = [&](const Dictionary& d) -> std::string {
    //     std::ostringstream ss;
    //     ss << "{ ";
    //     bool first = true;
    //     for (auto const& p : d.items()) {
    //         if (!first) ss << ", ";
    //         first = false;
    //         ss << '"' << p.first << '"' << ": ";
    //         const Value& val = p.second;
    //         if (val.isDict()) {
    //             ss << make_pretty_compact(*val.asDict());
    //         } else if (val.isList()) {
    //             const auto& L = val.asList();
    //             ss << '[';
    //             for (size_t i = 0; i < L.size(); ++i) {
    //                 if (i) ss << ", ";
    //                 if (L[i].isDict())
    //                     ss << make_pretty_compact(*L[i].asDict());
    //                 else if (L[i].isList())
    //                     ss << L[i].dump(0, compact);
    //                 else
    //                     ss << L[i].to_string();
    //             }
    //             ss << ']';
    //         } else {
    //             ss << val.to_string();
    //         }
    //     }
    //     ss << " }";
    //     return ss.str();
    // };

    // bool force_expand = false;
    // if (compact) {
    //     std::string one = make_pretty_compact(*this);
    //     if (one.size() > 80) force_expand = true;
    // }

    // std::ostringstream out;

    // // Forward declarations of small helpers used by the pretty printer
    // std::function<void(const Value&, int)> dumpValue;
    // std::function<void(const Dictionary&, int)> dumpObject;

    // dumpValue = [&](const Value& val, int level) {
    //     if (val.isNull()) {
    //         out << "null";
    //         return;
    //     }
    //     if (val.isInt()) {
    //         out << val.asInt();
    //         return;
    //     }
    //     if (val.isDouble()) {
    //         std::ostringstream ss;
    //         ss << val.asDouble();
    //         out << ss.str();
    //         return;
    //     }
    //     if (val.isBool()) {
    //         out << (val.asBool() ? "true" : "false");
    //         return;
    //     }
    //     if (val.isString()) {
    //         out << '"' << val.asString() << '"';
    //         return;
    //     }
    //     if (val.isList()) {
    //         const auto& L = val.asList();
    //         if (L.empty()) {
    //             out << "[]";
    //             return;
    //         }
    //         if (compact) {
    //             out << '[';
    //             for (size_t i = 0; i < L.size(); ++i) {
    //                 out << L[i].dump(0, compact);
    //                 if (i + 1 < L.size()) out << ", ";
    //             }
    //             out << ']';
    //             return;
    //         }
    //         out << "[\n";
    //         for (size_t i = 0; i < L.size(); ++i) {
    //             out << std::string(static_cast<size_t>(level + indent), ' ');
    //             dumpValue(L[i], level + indent);
    //             if (i + 1 < L.size())
    //                 out << ",\n";
    //             else
    //                 out << "\n";
    //         }
    //         out << std::string(static_cast<size_t>(level), ' ') << "]";
    //         return;
    //     }
    //     if (val.isDict()) {
    //         dumpObject(*val.asDict(), level);
    //         return;
    //     }
    //     out << "null";
    // };

    // dumpObject = [&](const Dictionary& d, int level) {
    //     auto items = d.items();
    //     if (items.empty()) {
    //         out << "{}";
    //         return;
    //     }
    //     if (compact && !force_expand) {
    //         std::string one = make_pretty_compact(d);
    //         if (one.size() <= 80) {
    //             out << one;
    //             return;
    //         }
    //     }
    //     out << "{\n";
    //     for (size_t i = 0; i < items.size(); ++i) {
    //         out << std::string(static_cast<size_t>(level + indent), ' ') << '"' << items[i].first
    //             << '"' << ": ";
    //         dumpValue(items[i].second, level + indent);
    //         if (i + 1 < items.size())
    //             out << ",\n";
    //         else
    //             out << "\n";
    //     }
    //     out << std::string(static_cast<size_t>(level), ' ') << "}";
    // };

    // dumpObject(*this, 0);
    // return out.str();
    return "";
}

inline Dictionary Dictionary::overrideEntries(const Dictionary& config) const {
    // Create a copy of this dictionary and apply entries from config,
    // overriding values where provided. If both values are objects, recurse.
    Dictionary out = *this;
    if (config.my_type != TYPE::Object) return out;
    for (auto const& p : config.m_object_map) {
        auto it = out.m_object_map.find(p.first);
        if (it != out.m_object_map.end() && it->second.my_type == TYPE::Object && p.second.my_type == TYPE::Object) {
            it->second = it->second.overrideEntries(p.second);
        } else {
            out.m_object_map[p.first] = p.second;
        }
    }
    return out;
    // Dictionary out = *this;
    // for (auto const& p : config.data) {
    //     auto it = out.data.find(p.first);
    //     if (it != out.data.end() && it->second.isDict() && p.second.isDict()) {
    //         Dictionary merged = *it->second.asDict();
    //         merged = merged.overrideEntries(*p.second.asDict());
    //         out.data[p.first] = Value(merged);
    //     } else {
    //         out.data[p.first] = p.second;
    //     }
    // }
    // if (config.scalar) out.scalar = config.scalar;
    // return out;
}

Dictionary removeDefaultsFromMappedObject(const Dictionary& defaults, const Dictionary& user_options);

inline Dictionary removeDefaultsFromObjectArray(const Dictionary& defaults, const Dictionary& user_options) {
    if (user_options.type() != Dictionary::ObjectArray) {
        throw std::logic_error(std::string("expected array object in user options but got a " +
                                           user_options.typeString() + " <" + user_options.dump() + ">"));
    }

    if (not defaults.isArrayObject()) {
        throw std::logic_error(std::string("expected array object in defaults but got a " + defaults.typeString() +
                                           " <" + defaults.dump() + ">"));
    }

    Dictionary result;
    for (int i = 0; i < user_options.size(); ++i) {
        const auto& opt = user_options[i];
        if (i >= defaults.size()) {
            result[i] = opt;
        } else if (opt.isValueObject()) {
            if (opt != defaults[i]) result[i] = opt;
        } else if (opt.isMappedObject()) {
            result[i] = removeDefaultsFromMappedObject(defaults[i], opt);
        } else if (opt.type() == Dictionary::ObjectArray) {
            result[i] = removeDefaultsFromObjectArray(defaults[i], opt);
        } else if (opt.isArrayObject()) {
            if (opt != defaults[i]) result[i] = opt;
        }
    }

    return result;
}

inline Dictionary removeDefaultsFromMappedObject(const Dictionary& defaults, const Dictionary& user_options) {
    if (not user_options.isMappedObject()) {
        throw std::logic_error(std::string("user options are not a mapped object: " + user_options.typeString()));
    }
    if (not defaults.isMappedObject()) {
        throw std::logic_error(std::string("defaults are not a mapped object: " + defaults.typeString()));
    }

    Dictionary result;
    for (const auto& key : user_options.keys()) {
        const auto& opt = user_options[key];
        if (not defaults.has(key)) {
            result[key] = opt;
        } else if (opt.isValueObject()) {
            if (opt != defaults[key]) result[key] = opt;
        } else if (opt.isMappedObject()) {
            result[key] = removeDefaultsFromMappedObject(defaults[key], opt);
        } else if (opt.type() == Dictionary::ObjectArray) {
            result[key] = removeDefaultsFromObjectArray(defaults[key], opt);
        } else if (opt.isArrayObject()) {
            if (opt != defaults[key]) result[key] = opt;
        }
        if (result[key].isMappedObject() and result[key].size() == 0) {
            result.erase(key);
        }
    }

    return result;
}

inline Dictionary Dictionary::removeCommonEntries(const Dictionary& config) const {
    Dictionary result;
    if (isMappedObject()) {
        result = removeDefaultsFromMappedObject(config, *this);
    } else if (type() == ObjectArray) {
        result = removeDefaultsFromObjectArray(config, *this);
    } else if (isArrayObject()) {
        if (*this != config) result = *this;
    }
    return result;
}

inline Dictionary Dictionary::merge(const Dictionary& config) const {
    // Similar to overrideEntries: return a new Dictionary with entries from
    // this merged with config; config values overwrite or are added. Nested
    // objects are merged recursively.
    Dictionary out = *this;
    if (config.my_type != TYPE::Object) return out;
    for (auto const& p : config.m_object_map) {
        auto it = out.m_object_map.find(p.first);
        if (it != out.m_object_map.end() && it->second.my_type == TYPE::Object && p.second.my_type == TYPE::Object) {
            it->second = it->second.merge(p.second);
        } else {
            out.m_object_map[p.first] = p.second;
        }
    }
    return out;
}

inline std::ostream& operator<<(std::ostream& os, const Dictionary& d) {
    os << d.dump();
    return os;
}
}  // namespace ps
