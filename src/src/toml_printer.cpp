#include <ps/toml.h>
#include <ps/dictionary.h>
#include <sstream>
#include <string>
#include <stdexcept>
#include <cctype>

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

static void emit_value(std::ostringstream& out, const Dictionary& val);

static void emit_array(std::ostringstream& out, const Dictionary& val) {
    out << '[';
    for (int i = 0; i < val.size(); ++i) {
        if (i > 0) out << ", ";
        emit_value(out, val.at(i));
    }
    out << ']';
}

static void emit_value(std::ostringstream& out, const Dictionary& val) {
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
            // Array of tables - emit as inline tables for simplicity
            out << '[';
            for (int i = 0; i < val.size(); ++i) {
                if (i > 0) out << ", ";
                const auto& obj = val.at(i);
                out << '{';
                auto keys = obj.keys();
                for (size_t j = 0; j < keys.size(); ++j) {
                    if (j > 0) out << ", ";
                    out << format_key(keys[j]) << " = ";
                    emit_value(out, obj.at(keys[j]));
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
                emit_value(out, val.at(keys[i]));
            }
            out << '}';
            break;
    }
}

std::string dump_toml(const Dictionary& d) {
    std::ostringstream out;

    if (!d.isMappedObject()) {
        throw std::runtime_error("TOML root must be a table (object), not an array or scalar");
    }

    auto keys = d.keys();
    
    // First, emit simple key-value pairs
    for (const auto& key : keys) {
        const auto& val = d.at(key);
        
        // Skip tables and array-of-tables for the first pass
        if (val.isMappedObject() || val.isArrayObject()) {
            if (val.isArrayObject() && val.size() > 0 && val.at(0).isMappedObject()) {
                continue; // Array of tables, handle in second pass
            }
            if (val.isMappedObject()) {
                continue; // Table, handle in second pass
            }
        }
        
        out << format_key(key) << " = ";
        emit_value(out, val);
        out << '\n';
    }

    // Second, emit tables
    for (const auto& key : keys) {
        const auto& val = d.at(key);
        
        if (val.isMappedObject()) {
            out << '\n';
            out << '[' << format_key(key) << "]\n";
            auto subkeys = val.keys();
            for (const auto& subkey : subkeys) {
                out << format_key(subkey) << " = ";
                emit_value(out, val.at(subkey));
                out << '\n';
            }
        }
    }

    // Third, emit array of tables
    for (const auto& key : keys) {
        const auto& val = d.at(key);
        
        if (val.isArrayObject() && val.size() > 0 && val.at(0).isMappedObject()) {
            for (int i = 0; i < val.size(); ++i) {
                out << '\n';
                out << "[[" << format_key(key) << "]]\n";
                const auto& table = val.at(i);
                auto subkeys = table.keys();
                for (const auto& subkey : subkeys) {
                    out << format_key(subkey) << " = ";
                    emit_value(out, table.at(subkey));
                    out << '\n';
                }
            }
        }
    }

    return out.str();
}

}  // namespace ps
