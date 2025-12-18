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

struct DictionaryScalarImpl {
    bool m_bool = false;
    double m_double = 0.0;
    int64_t m_int = 0;
    std::string m_string = "";
};
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

  private:
    TYPE my_type = Object;
    std::shared_ptr<DictionaryScalarImpl> scalar = std::make_shared<DictionaryScalarImpl>();

    std::map<int, Dictionary> m_array_map;
    std::map<std::string, Dictionary> m_object_map;

  public:
    Dictionary() { my_type = TYPE::Object; }
    ~Dictionary() = default;

    // Deep-copy constructor: delegate to operator= to ensure maps and
    // scalar state are copied deeply (no shared state with source).
    Dictionary(const Dictionary& d) { *this = d; }

    Dictionary(const std::string& s) {
        my_type = TYPE::String;
        scalar->m_string = s;
    }

    Dictionary(const char* s) { *this = std::string(s); }

    Dictionary(int64_t n) {
        my_type = TYPE::Integer;
        scalar->m_int = n;
    }

    Dictionary(int n) { *this = int64_t(n); }

    Dictionary(double x) {
        my_type = TYPE::Double;
        scalar->m_double = x;
    }

    Dictionary(bool b) {
        my_type = TYPE::Boolean;
        scalar->m_bool = b;
    }

    Dictionary(const std::vector<double>& v) {
        my_type = TYPE::DoubleArray;
        for (size_t i = 0; i < v.size(); ++i)
            m_array_map.emplace(static_cast<int>(i), Dictionary(v[i]));
    }

    Dictionary(const std::vector<int>& v) {
        my_type = TYPE::IntArray;
        for (size_t i = 0; i < v.size(); ++i)
            m_array_map.emplace(static_cast<int>(i), Dictionary(static_cast<int64_t>(v[i])));
    }

    Dictionary(const std::vector<std::string>& v) {
        my_type = TYPE::StringArray;
        for (size_t i = 0; i < v.size(); ++i)
            m_array_map.emplace(static_cast<int>(i), Dictionary(v[i]));
    }

    Dictionary(const std::vector<bool>& v) {
        my_type = TYPE::BoolArray;
        for (size_t i = 0; i < v.size(); ++i)
            m_array_map.emplace(static_cast<int>(i), Dictionary(v[i]));
    }

    Dictionary(const std::vector<Dictionary>& v) {
        my_type = TYPE::ObjectArray;
        for (size_t i = 0; i < v.size(); ++i)
            m_array_map.emplace(static_cast<int>(i), Dictionary(v[i]));
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
              typename = std::enable_if_t<
                          !std::is_same<std::decay_t<T>,
                                        std::pair<std::string, Dictionary> >::value> >
    Dictionary(std::initializer_list<T> init) {
        my_type = TYPE::ObjectArray;
        int i = 0;
        for (auto const& v : init) m_array_map.emplace(i++, Dictionary(v));
    }

    static Dictionary null() {
        Dictionary d;
        d.my_type = TYPE::Null;
        return d;
    }

    Dictionary& operator=(const Dictionary& d) {
        if (this == &d) return *this;
        my_type = d.my_type;
        // deep-copy the scalar value so this Dictionary does not share state
        scalar = std::make_shared<DictionaryScalarImpl>(*d.scalar);

        // Build new maps first (do not modify `this` while reading `d`),
        // so assigning from a sub-element (e.g. `dict = dict["key"]`) is safe.
        std::map<std::string, Dictionary> new_object_map;
        std::map<int, Dictionary> new_array_map;

        switch (my_type) {
            case TYPE::Object:
                for (auto const& p : d.m_object_map) {
                    Dictionary tmp;  // default construct then assign to get deep-copy
                    tmp = p.second;
                    new_object_map.emplace(p.first, std::move(tmp));
                }
                break;
            case TYPE::ObjectArray:
            case TYPE::IntArray:
            case TYPE::DoubleArray:
            case TYPE::StringArray:
            case TYPE::BoolArray:
                for (auto const& kv : d.m_array_map) {
                    Dictionary tmp;
                    tmp = kv.second;
                    new_array_map.emplace(kv.first, std::move(tmp));
                }
                break;
            case TYPE::Null:
                // nothing extra to copy
                break;
            default:
                // scalar already copied above for scalar types
                break;
        }

        // Now swap in the newly constructed maps.
        if (!new_object_map.empty() || my_type == TYPE::Object)
            m_object_map = std::move(new_object_map);
        if (!new_array_map.empty() || my_type == TYPE::ObjectArray || my_type == TYPE::IntArray ||
            my_type == TYPE::DoubleArray || my_type == TYPE::StringArray ||
            my_type == TYPE::BoolArray)
            m_array_map = std::move(new_array_map);

        return *this;
    }

    Dictionary& operator=(const std::string& s) {
        my_type = TYPE::String;
        scalar->m_string = s;
        return *this;
    }

    Dictionary& operator=(const char* s) { return operator=(std::string(s)); }

    Dictionary& operator=(int64_t n) {
        scalar->m_int = n;
        my_type = TYPE::Integer;
        return *this;
    }

    Dictionary& operator=(int n) { return operator=(int64_t(n)); }

    Dictionary& operator=(double x) {
        scalar->m_double = x;
        my_type = TYPE::Double;
        return *this;
    }

    Dictionary& operator=(const bool& b) {
        scalar->m_bool = b;
        my_type = TYPE::Boolean;
        return *this;
    }

    Dictionary& operator=(const std::vector<int>& v) {
        std::map<int, Dictionary> M;
        for (size_t i = 0; i < v.size(); ++i)
            M.emplace(static_cast<int>(i), Dictionary(int64_t(v[i])));
        m_array_map = std::move(M);
        my_type = TYPE::IntArray;
        return *this;
    }

    Dictionary& operator=(const std::vector<bool>& v) {
        std::map<int, Dictionary> M;
        for (size_t i = 0; i < v.size(); ++i) M.emplace(static_cast<int>(i), Dictionary(v[i]));
        m_array_map = std::move(M);
        my_type = TYPE::BoolArray;
        return *this;
    }

    Dictionary& operator=(const std::vector<double>& v) {
        std::map<int, Dictionary> M;
        for (size_t i = 0; i < v.size(); ++i) M.emplace(static_cast<int>(i), Dictionary(v[i]));
        m_array_map = std::move(M);
        my_type = TYPE::DoubleArray;
        return *this;
    }

    Dictionary& operator=(const std::vector<std::string>& v) {
        std::map<int, Dictionary> M;
        for (size_t i = 0; i < v.size(); ++i) M.emplace(static_cast<int>(i), Dictionary(v[i]));
        m_array_map = std::move(M);
        my_type = TYPE::StringArray;
        return *this;
    }

    Dictionary& operator=(const std::vector<Dictionary>& v) {
        std::map<int, Dictionary> M;
        for (size_t i = 0; i < v.size(); ++i) M.emplace(static_cast<int>(i), v[i]);
        m_array_map = std::move(M);
        my_type = TYPE::ObjectArray;
        return *this;
    }

    Dictionary& operator=(std::vector<Dictionary>&& v) {
        std::map<int, Dictionary> M;
        for (size_t i = 0; i < v.size(); ++i) M.emplace(static_cast<int>(i), std::move(v[i]));
        m_array_map = std::move(M);
        my_type = TYPE::ObjectArray;
        return *this;
    }

    bool operator==(const Dictionary& rhs) const {
        if (my_type != rhs.my_type) return false;
        switch (my_type) {
            case TYPE::Boolean:
                return scalar->m_bool == rhs.scalar->m_bool;
            case TYPE::Double:
                return scalar->m_double == rhs.scalar->m_double;
            case TYPE::Integer:
                return scalar->m_int == rhs.scalar->m_int;
            case TYPE::String:
                return scalar->m_string == rhs.scalar->m_string;
            case TYPE::BoolArray:
            case TYPE::DoubleArray:
            case TYPE::IntArray:
            case TYPE::StringArray:
                return m_array_map == rhs.m_array_map;
            case TYPE::Object: {
                auto my_keys = keys();
                auto rhs_keys = rhs.keys();
                if (my_keys.size() != rhs_keys.size()) return false;
                for (auto const& k : my_keys) {
                    if (rhs.count(k) == 0) return false;
                    if (at(k) != rhs.at(k)) return false;
                }
                return true;
            }
            case TYPE::ObjectArray:
                return m_array_map == rhs.m_array_map;
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
                return static_cast<int>(m_array_map.size());
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
                return m_array_map.empty();
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
        m_array_map.clear();
        my_type = TYPE::Object;
    }

    TYPE type() const {
        // If this is an object-array created via `operator[]` access,
        // infer a more specific homogeneous array type where possible
        // (e.g., all-integers -> IntArray, all-numbers -> DoubleArray,
        // all-strings -> StringArray, all-bools -> BoolArray).
        if (my_type != TYPE::ObjectArray) return my_type;
        if (m_array_map.empty()) return my_type;

        bool all_int = true;
        bool all_double_or_int = true;
        bool all_string = true;
        bool all_bool = true;
        bool all_object = true;

        for (auto const& kv : m_array_map) {
            const Dictionary& el = kv.second;
            switch (el.my_type) {
                case TYPE::Integer:
                    all_string = false;
                    all_bool = false;
                    all_object = false;
                    break;
                case TYPE::Double:
                    all_int = false;
                    all_string = false;
                    all_bool = false;
                    all_object = false;
                    break;
                case TYPE::IntArray:
                    // nested arrays break scalar-homogeneity
                    all_int = false;
                    all_double_or_int = false;
                    all_string = false;
                    all_bool = false;
                    all_object = false;
                    break;
                case TYPE::DoubleArray:
                    all_int = false;
                    all_double_or_int = false;
                    all_string = false;
                    all_bool = false;
                    all_object = false;
                    break;
                case TYPE::StringArray:
                    all_int = false;
                    all_double_or_int = false;
                    all_string = false;
                    all_bool = false;
                    all_object = false;
                    break;
                case TYPE::BoolArray:
                    all_int = false;
                    all_double_or_int = false;
                    all_string = false;
                    all_bool = false;
                    all_object = false;
                    break;
                case TYPE::String:
                    all_int = false;
                    all_double_or_int = false;
                    all_bool = false;
                    all_object = false;
                    break;
                case TYPE::Boolean:
                    all_int = false;
                    all_double_or_int = false;
                    all_string = false;
                    all_object = false;
                    break;
                case TYPE::Object:
                case TYPE::ObjectArray:
                    // Any nested object-like element forces ObjectArray
                    all_int = false;
                    all_double_or_int = false;
                    all_string = false;
                    all_bool = false;
                    break;
                case TYPE::Null:
                    // treat null as breaking homogeneous typed arrays
                    all_int = false;
                    all_double_or_int = false;
                    all_string = false;
                    all_bool = false;
                    all_object = false;
                    break;
            }
        }

        if (all_int) return TYPE::IntArray;
        if (all_double_or_int) return TYPE::DoubleArray;
        if (all_string) return TYPE::StringArray;
        if (all_bool) return TYPE::BoolArray;
        if (all_object) return TYPE::ObjectArray;
        return TYPE::ObjectArray;
    }

    std::string typeString() const {
        switch (my_type) {
            case TYPE::Object:
                return "Object";
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

    // Proxy returned for mutable integer-index access so we can observe
    // assignments like `dict["arr"][0] = 3;` and promote the parent
    // container type to an appropriate array type (IntArray, DoubleArray,
    // StringArray, BoolArray, ObjectArray).
    Dictionary& operator[](int index) {
        // If this is an object that has never been used as a mapped object,
        // allow converting it to an array on first integer-index access so
        // usage like dict["arr"][0] = 5 works naturally.
        if (my_type == TYPE::Object && m_object_map.empty()) {
            my_type = TYPE::ObjectArray;
            m_array_map.clear();
        }
        if (my_type == TYPE::ObjectArray || my_type == TYPE::IntArray ||
            my_type == TYPE::DoubleArray || my_type == TYPE::StringArray ||
            my_type == TYPE::BoolArray) {
            if (index < 0) throw std::logic_error("Negative index");
            for (int i = 0; i <= index; ++i) {
                if (m_array_map.find(i) == m_array_map.end()) m_array_map.emplace(i, Dictionary());
            }
            return m_array_map[index];
        }
        throw std::logic_error("Not a list");
    }

    const Dictionary& operator[](int index) const {
        if (my_type == TYPE::ObjectArray || my_type == TYPE::IntArray ||
            my_type == TYPE::DoubleArray || my_type == TYPE::StringArray ||
            my_type == TYPE::BoolArray) {
            return m_array_map.at(index);
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
        if (my_type == TYPE::ObjectArray || my_type == TYPE::IntArray ||
            my_type == TYPE::DoubleArray || my_type == TYPE::StringArray ||
            my_type == TYPE::BoolArray) {
            return m_array_map.at(index);
        }
        throw std::logic_error("Not a list");
    }

    const Dictionary& at(int index) const {
        if (my_type == TYPE::ObjectArray || my_type == TYPE::IntArray ||
            my_type == TYPE::DoubleArray || my_type == TYPE::StringArray ||
            my_type == TYPE::BoolArray) {
            return m_array_map.at(index);
        }
        throw std::logic_error("Not a list");
    }

    const Dictionary& at(const std::string& k) const {
        auto it = m_object_map.find(k);
        if (it != m_object_map.end()) return it->second;

        // didn't find it, throw a decent error message
        std::ostringstream ss;
        ss << "Could not find key <" << k << "> available options are: ";
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

        // didn't find it, throw a decent error message
        std::ostringstream ss;
        ss << "Could not find key <" << k << "> available options are: ";
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
            return {};
            // throw std::logic_error("Cannot get keys of non-object type");
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
        if (my_type == TYPE::String) return scalar->m_string;
        if (my_type == TYPE::Integer) return std::to_string(scalar->m_int);
        if (my_type == TYPE::Double) {
            std::ostringstream ss;
            ss << scalar->m_double;
            return ss.str();
        }
        if (my_type == TYPE::Boolean) return scalar->m_bool ? "true" : "false";
        throw std::runtime_error("not a string");
    }

    std::array<double, 3> asPoint() const {
        if (my_type == TYPE::IntArray or my_type == TYPE::DoubleArray) {
            if (m_array_map.size() == 3) {
                std::array<double, 3> out;
                out[0] = m_array_map.at(0).asDouble();
                out[1] = m_array_map.at(1).asDouble();
                out[2] = m_array_map.at(2).asDouble();
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
    std::vector<std::string> getStrings(const std::string& key) const {
        return at(key).asStrings();
    }
    std::vector<bool> getBools(const std::string& key) const { return at(key).asBools(); }

    int asInt() const {
        if (my_type == TYPE::Integer) return static_cast<int>(scalar->m_int);
        if (my_type == TYPE::Double) return static_cast<int>(scalar->m_double);
        throw std::runtime_error("not an int");
    }

    double asDouble() const {
        if (my_type == TYPE::Double) return scalar->m_double;
        if (my_type == TYPE::Integer) return static_cast<double>(scalar->m_int);
        throw std::runtime_error("not a double");
    }

    bool asBool() const {
        if (my_type == TYPE::Boolean) return scalar->m_bool;
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
        if (my_type == TYPE::Integer) return static_cast<int>(scalar->m_int) == rhs;
        if (my_type == TYPE::Double) return static_cast<int>(scalar->m_double) == rhs;
        return false;
    }

    bool operator!=(int rhs) const { return not(*this == rhs); }

    bool operator==(double rhs) const {
        if (my_type == TYPE::Double) return scalar->m_double == rhs;
        if (my_type == TYPE::Integer) return static_cast<double>(scalar->m_int) == rhs;
        return false;
    }

    bool operator!=(double rhs) const { return not(*this == rhs); }

    bool operator==(bool rhs) const {
        if (my_type == TYPE::Boolean) return scalar->m_bool == rhs;
        return false;
    }

    bool operator!=(bool rhs) const { return not(*this == rhs); }

    std::string dump(int indent = 0, bool compact = true) const;

    Dictionary overrideEntries(const Dictionary& config) const;

    Dictionary merge(const Dictionary& config) const;

    Dictionary removeCommonEntries(const Dictionary& config) const;

    // convenience overloads that accept Value

    std::string to_string() const {
        switch (my_type) {
            case TYPE::Null:
                return "null";
            case TYPE::Boolean:
                return scalar->m_bool ? "true" : "false";
            case TYPE::Integer:
                return std::to_string(scalar->m_int);
            case TYPE::Double:
                return std::to_string(scalar->m_double);
            default:
                break;
        }
        return dump();
    }
};

inline std::vector<int> Dictionary::asInts() const {
    if (my_type == TYPE::Integer) {
        return std::vector<int>{static_cast<int>(scalar->m_int)};
    }
    if (my_type == TYPE::Double) {
        return std::vector<int>{static_cast<int>(scalar->m_double)};
    }
    if (my_type == TYPE::IntArray) {
        std::vector<int> out;
        out.reserve(m_array_map.size());
        for (auto const& kv : m_array_map) out.push_back(kv.second.asInt());
        return out;
    }
    if (my_type == TYPE::DoubleArray) {
        // TODO warn if double isn't approx an int
        std::vector<int> out;
        out.reserve(m_array_map.size());
        for (auto const& kv : m_array_map) out.push_back(static_cast<int>(kv.second.asDouble()));
        return out;
    }
    if (my_type == TYPE::ObjectArray) {
        std::vector<int> out;
        out.reserve(m_array_map.size());
        for (auto const& kv : m_array_map) out.push_back(kv.second.asInt());
        return out;
    }
    throw std::runtime_error("not an int list");
}

inline std::vector<double> Dictionary::asDoubles() const {
    if (my_type == TYPE::Double) {
        return std::vector<double>{scalar->m_double};
    }
    if (my_type == TYPE::Integer) {
        return std::vector<double>{static_cast<double>(scalar->m_int)};
    }
    if (my_type == TYPE::DoubleArray) {
        std::vector<double> out;
        out.reserve(m_array_map.size());
        for (auto const& kv : m_array_map) out.push_back(kv.second.asDouble());
        return out;
    }
    if (my_type == TYPE::IntArray) {
        std::vector<double> out;
        out.reserve(m_array_map.size());
        for (auto const& kv : m_array_map) out.push_back(static_cast<double>(kv.second.asInt()));
        return out;
    }
    if (my_type == TYPE::ObjectArray) {
        std::vector<double> out;
        out.reserve(m_array_map.size());
        for (auto const& kv : m_array_map) out.push_back(kv.second.asDouble());
        return out;
    }
    throw std::runtime_error("not a double list");
}

inline std::vector<std::string> Dictionary::asStrings() const {
    if (my_type == TYPE::String) {
        return std::vector<std::string>{scalar->m_string};
    }
    if (my_type == TYPE::StringArray) {
        std::vector<std::string> out;
        out.reserve(m_array_map.size());
        for (auto const& kv : m_array_map) out.push_back(kv.second.asString());
        return out;
    }
    if (my_type == TYPE::ObjectArray) {
        std::vector<std::string> out;
        out.reserve(m_array_map.size());
        for (auto const& kv : m_array_map) out.push_back(kv.second.asString());
        return out;
    }
    throw std::runtime_error("not a string list");
}

inline std::vector<bool> Dictionary::asBools() const {
    if (my_type == TYPE::Boolean) {
        return std::vector<bool>{scalar->m_bool};
    }
    if (my_type == TYPE::BoolArray) {
        std::vector<bool> out;
        out.reserve(m_array_map.size());
        for (auto const& kv : m_array_map) out.push_back(kv.second.asBool());
        return out;
    }
    if (my_type == TYPE::ObjectArray) {
        std::vector<bool> out;
        out.reserve(m_array_map.size());
        for (auto const& kv : m_array_map) out.push_back(kv.second.asBool());
        return out;
    }
    throw std::runtime_error("not a bool list");
}

inline std::vector<Dictionary> Dictionary::asObjects() const {
    if (my_type == TYPE::ObjectArray) {
        std::vector<Dictionary> out;
        out.reserve(m_array_map.size());
        for (auto const& kv : m_array_map) out.push_back(kv.second);
        return out;
    }
    if (my_type == TYPE::Object) {
        return std::vector<Dictionary>{*this};
    }
    throw std::runtime_error("not an object list");
}

// Helper function to escape JSON strings
static inline std::string escape_json_string(const std::string& s) {
    std::string result;
    result.reserve(s.size() + 2);
    result.push_back('"');
    for (char c : s) {
        switch (c) {
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            case '\b':
                result += "\\b";
                break;
            case '\f':
                result += "\\f";
                break;
            default:
                result.push_back(c);
                break;
        }
    }
    result.push_back('"');
    return result;
}

inline std::string Dictionary::dump(int indent, bool compact) const {
    std::function<std::string(const Dictionary&)> make_pretty_compact;
    make_pretty_compact = [&](const Dictionary& d) -> std::string {
        switch (d.my_type) {
            case TYPE::Null:
                return "null";
            case TYPE::Boolean:
                return d.scalar->m_bool ? "true" : "false";
            case TYPE::Integer:
                return std::to_string(d.scalar->m_int);
            case TYPE::Double: {
                std::ostringstream ss;
                ss << d.scalar->m_double;
                return ss.str();
            }
            case TYPE::String:
                return escape_json_string(d.scalar->m_string);
            case TYPE::IntArray:
            case TYPE::DoubleArray:
            case TYPE::StringArray:
            case TYPE::BoolArray:
            case TYPE::ObjectArray: {
                std::ostringstream ss;
                ss << '[';
                bool first = true;
                std::vector<Dictionary> elems;
                elems.reserve(d.m_array_map.size());
                for (auto const& kv : d.m_array_map) elems.push_back(kv.second);
                for (auto const& el : elems) {
                    if (!first) ss << ",";
                    first = false;
                    ss << make_pretty_compact(el);
                }
                ss << ']';
                return ss.str();
            }
            case TYPE::Object: {
                if (d.m_object_map.empty()) return std::string("{}");
                std::ostringstream ss;
                ss << '{';
                bool first = true;
                for (auto const& p : d.m_object_map) {
                    if (!first) ss << ",";
                    first = false;
                    ss << '"' << p.first << '"' << (compact ? ":" : ": ")
                       << make_pretty_compact(p.second);
                }
                ss << '}';
                return ss.str();
            }
        }
        return std::string();
    };

    bool force_expand = false;
    // If compact output was requested and no indent was provided, then
    // we may return a single-line compact representation when it's short.
    if (compact && indent == 0) {
        std::string one = make_pretty_compact(*this);
        if (one.size() > 80) force_expand = true;
        if (!force_expand && one.size() <= 80) return one;
    }
    // When an explicit indent is requested, prefer pretty-printed objects
    // (multi-line with indentation) but keep arrays compact by default.
    bool compactObjects = compact;
    if (indent > 0) compactObjects = false;

    std::ostringstream out;

    std::function<void(const Dictionary&, int)> dumpValue;
    std::function<void(const Dictionary&, int)> dumpObject;

    dumpValue = [&](const Dictionary& val, int level) {
        switch (val.my_type) {
            case TYPE::Null:
                out << "null";
                return;
            case TYPE::Boolean:
                out << (val.scalar->m_bool ? "true" : "false");
                return;
            case TYPE::Integer:
                out << val.scalar->m_int;
                return;
            case TYPE::Double: {
                std::ostringstream ss;
                ss << val.scalar->m_double;
                out << ss.str();
                return;
            }
            case TYPE::String:
                out << escape_json_string(val.scalar->m_string);
                return;
            case TYPE::IntArray:
            case TYPE::DoubleArray:
            case TYPE::StringArray:
            case TYPE::BoolArray:
            case TYPE::ObjectArray: {
                std::vector<Dictionary> L;
                L.reserve(val.m_array_map.size());
                for (auto const& kv : val.m_array_map) L.push_back(kv.second);
                if (L.empty()) {
                    out << "[]";
                    return;
                }
                // When compact output is requested, print arrays inline with
                // no spaces between elements (e.g. "[1,2,3]"). Also, for
                // readability, allow small arrays to be printed inline with
                // the same no-space format even when `compact` is false.
                if (compact) {
                    out << '[';
                    for (size_t i = 0; i < L.size(); ++i) {
                        if (i) out << ",";
                        out << make_pretty_compact(L[i]);
                    }
                    out << ']';
                    return;
                }

                // Print small arrays inline (no spaces) even when not in
                // compact mode. We consider arrays of up to 3 elements "small".
                if (!compact && L.size() <= 3) {
                    out << '[';
                    for (size_t i = 0; i < L.size(); ++i) {
                        if (i) out << ",";
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
        if (compactObjects && !force_expand) {
            std::string one = make_pretty_compact(d);
            if (one.size() <= 80) {
                out << one;
                return;
            }
        }
        out << "{\n";
        auto items = d.items();
        for (size_t i = 0; i < items.size(); ++i) {
            out << std::string(static_cast<size_t>(level + indent), ' ') << '"' << items[i].first
                << '"' << (compactObjects ? ":" : ": ");
            dumpValue(items[i].second, level + indent);
            if (i + 1 < items.size())
                out << ",\n";
            else
                out << "\n";
        }
        out << std::string(static_cast<size_t>(level), ' ') << "}";
    };

    dumpValue(*this, 0);
    return out.str();
}

inline Dictionary Dictionary::overrideEntries(const Dictionary& config) const {
    // Create a copy of this dictionary and apply entries from config,
    // overriding values where provided. If both values are objects, recurse.
    Dictionary out = *this;
    if (config.my_type != TYPE::Object) return out;
    for (auto const& p : config.m_object_map) {
        auto it = out.m_object_map.find(p.first);
        if (it != out.m_object_map.end() && it->second.my_type == TYPE::Object &&
            p.second.my_type == TYPE::Object) {
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

Dictionary removeDefaultsFromMappedObject(const Dictionary& defaults,
                                          const Dictionary& user_options);

inline Dictionary removeDefaultsFromObjectArray(const Dictionary& defaults,
                                                const Dictionary& user_options) {
    if (user_options.type() != Dictionary::ObjectArray) {
        throw std::logic_error(std::string("expected array object in user options but got a " +
                                           user_options.typeString() + " <" + user_options.dump() +
                                           ">"));
    }

    if (not defaults.isArrayObject()) {
        throw std::logic_error(std::string("expected array object in defaults but got a " +
                                           defaults.typeString() + " <" + defaults.dump() + ">"));
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

inline Dictionary removeDefaultsFromMappedObject(const Dictionary& defaults,
                                                 const Dictionary& user_options) {
    if (not user_options.isMappedObject()) {
        throw std::logic_error(std::string("user options are not a mapped object: " +
                                           user_options.typeString()));
    }
    if (not defaults.isMappedObject()) {
        throw std::logic_error(
                    std::string("defaults are not a mapped object: " + defaults.typeString()));
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
        if (it != out.m_object_map.end() && it->second.my_type == TYPE::Object &&
            p.second.my_type == TYPE::Object) {
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
