#include <ps/toml.h>
#include <ps/dictionary.h>
#include <sstream>
#include <string>
#include <stdexcept>
#include <cctype>
#include <vector>

namespace ps {

static std::string escape_string_toml(const std::string& s) {
    std::ostringstream ss;
    ss << '"';
    for (char c : s) {
        if (c == '"' || c == '\\') {
            ss << '\\' << c;
        } else if (c == '\n') {
            ss << "\\n";
        } else if (c == '\t') {
            ss << "\\t";
        } else if (c == '\r') {
            ss << "\\r";
        } else {
            ss << c;
        }
    }
    ss << '"';
    return ss.str();
}

static bool is_valid_toml_key(const std::string& key) {
    if (key.empty()) return false;
    for (char c : key) {
        if (!std::isalnum(c) && c != '_' && c != '-') {
            return false;
        }
    }
    return true;
}

static std::string format_key(const std::string& key) {
    if (is_valid_toml_key(key)) {
        return key;
    }
    return escape_string_toml(key);
}

// Forward declarations
static void emit_simple_value(std::ostringstream& out, const Dictionary& val);
static void emit_dotted_object(std::ostringstream& out, const Dictionary& obj, const std::string& prefix);

static void emit_array(std::ostringstream& out, const Dictionary& val) {
    out << '[';
    for (int i = 0; i < val.size(); ++i) {
        if (i > 0) out << ", ";
        emit_simple_value(out, val.at(i));
    }
    out << ']';
}

// Emit simple values (no nested dotted keys)
static void emit_simple_value(std::ostringstream& out, const Dictionary& val) {
    switch (val.type()) {
        case Dictionary::TYPE::Null:
            throw std::runtime_error("TOML does not support null values");
        case Dictionary::TYPE::Boolean:
            out << (val.asBool() ? "true" : "false");
            break;
        case Dictionary::TYPE::Integer:
            out << val.asInt();
            break;
        case Dictionary::TYPE::Double:
            out << val.asDouble();
            break;
        case Dictionary::TYPE::String:
            out << escape_string_toml(val.asString());
            break;
        case Dictionary::TYPE::IntArray:
        case Dictionary::TYPE::DoubleArray:
        case Dictionary::TYPE::StringArray:
        case Dictionary::TYPE::BoolArray:
            emit_array(out, val);
            break;
        case Dictionary::TYPE::ObjectArray:
            // Array of objects - for simple values, use inline tables
            out << '[';
            for (int i = 0; i < val.size(); ++i) {
                if (i > 0) out << ", ";
                const auto& obj = val.at(i);
                out << '{';
                auto keys = obj.keys();
                for (size_t j = 0; j < keys.size(); ++j) {
                    if (j > 0) out << ", ";
                    out << format_key(keys[j]) << " = ";
                    emit_simple_value(out, obj.at(keys[j]));
                }
                out << '}';
            }
            out << ']';
            break;
        case Dictionary::TYPE::Object:
            // Inline table
            out << '{';
            auto keys = val.keys();
            for (size_t i = 0; i < keys.size(); ++i) {
                if (i > 0) out << ", ";
                out << format_key(keys[i]) << " = ";
                emit_simple_value(out, val.at(keys[i]));
            }
            out << '}';
            break;
    }
}

// Recursively emit dotted keys for nested objects
static void emit_dotted_object(std::ostringstream& out, const Dictionary& obj, const std::string& prefix) {
    auto keys = obj.keys();
    for (const auto& key : keys) {
        const auto& val = obj.at(key);
        std::string full_key = prefix.empty() ? format_key(key) : prefix + "." + format_key(key);
        
        if (val.isMappedObject()) {
            // Recurse into nested object with dotted syntax
            emit_dotted_object(out, val, full_key);
        } else {
            // Emit the value
            out << full_key << " = ";
            emit_simple_value(out, val);
            out << '\n';
        }
    }
}

std::string dump_toml(const Dictionary& d) {
    std::ostringstream out;

    if (!d.isMappedObject()) {
        throw std::runtime_error("TOML root must be a table (object), not an array or scalar");
    }

    auto keys = d.keys();
    
    // First pass: emit simple key-value pairs (non-tables, non-array-of-tables)
    for (const auto& key : keys) {
        const auto& val = d.at(key);
        
        // Skip tables and array-of-tables for the first pass
        if (val.isMappedObject()) {
            continue; // Table, handle in second pass
        }
        if (val.isArrayObject() && val.size() > 0 && val.at(0).isMappedObject()) {
            continue; // Array of tables, handle in third pass
        }
        
        out << format_key(key) << " = ";
        emit_simple_value(out, val);
        out << '\n';
    }

    // Second pass: emit tables with dotted key syntax for nested objects
    for (const auto& key : keys) {
        const auto& val = d.at(key);
        
        if (val.isMappedObject()) {
            out << '\n';
            out << '[' << format_key(key) << "]\n";
            emit_dotted_object(out, val, "");
        }
    }

    // Third pass: emit array of tables with dotted key syntax
    for (const auto& key : keys) {
        const auto& val = d.at(key);
        
        if (val.isArrayObject() && val.size() > 0 && val.at(0).isMappedObject()) {
            for (int i = 0; i < val.size(); ++i) {
                out << '\n';
                out << "[[" << format_key(key) << "]]\n";
                const auto& table = val.at(i);
                emit_dotted_object(out, table, "");
            }
        }
    }

    return out.str();
}

}  // namespace ps
