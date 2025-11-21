#include <ps/dictionary.h>
#include <sstream>
#include <string>
#include <functional>
#include <cctype>

namespace ps {

static bool needs_quoting(const std::string &s) {
    if (s.empty()) return true;
    // Leading or trailing whitespace should be quoted
    if (std::isspace(static_cast<unsigned char>(s.front())) ||
        std::isspace(static_cast<unsigned char>(s.back())))
        return true;

    // Characters that require quoting in many YAML contexts
    const std::string special = ":#{}[],&*?|-<>=!%@\\";
    for (char c : s) {
        if (c == '\n' || c == '\r') return true;
        if (special.find(c) != std::string::npos) return true;
    }

    // Strings that look like booleans/null or numbers should be quoted
    if (s == "null" || s == "Null" || s == "NULL" || s == "~") return true;
    if (s == "true" || s == "True" || s == "TRUE") return true;
    if (s == "false" || s == "False" || s == "FALSE") return true;

    // If it begins with a special indicator char like '*' or '&' or '>'
    if (!s.empty() && (s.front() == '*' || s.front() == '&' || s.front() == '>')) return true;

    return false;
}

static std::string quote_string(const std::string &s) {
    std::string out;
    out.push_back('"');
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
            out.push_back(c);
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\t') {
            out += "\\t";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('"');
    return out;
}

std::string dump_yaml(const Dictionary &dict) {
    std::ostringstream out;

    std::function<void(const Dictionary&, int)> emit;
    auto indent_spaces = [&](int n) {
        for (int k = 0; k < n; ++k) out.put(' ');
    };

    std::function<void(const Dictionary&, int, bool)> emit_inline_or_block;

    auto scalar_to_yaml = [&](const Dictionary &d) -> std::string {
        switch (d.type()) {
            case Dictionary::TYPE::Null:
                return std::string("null");
            case Dictionary::TYPE::Boolean:
                return d.asBool() ? "true" : "false";
            case Dictionary::TYPE::Integer:
                return std::to_string(d.asInt());
            case Dictionary::TYPE::Double: {
                std::ostringstream ss;
                ss << d.asDouble();
                return ss.str();
            }
            case Dictionary::TYPE::String: {
                std::string s = d.asString();
                if (needs_quoting(s)) return quote_string(s);
                return s;
            }
            default:
                break;
        }
        return std::string();
    };

    // emit_inline_or_block: when `inline_allowed` is true we attempt to emit
    // simple scalars inline (used after keys or list markers). For complex
    // containers we emit block style with increased indentation.
    emit_inline_or_block = [&](const Dictionary &d, int indent, bool inline_allowed) {
        switch (d.type()) {
            case Dictionary::TYPE::Null:
            case Dictionary::TYPE::Boolean:
            case Dictionary::TYPE::Integer:
            case Dictionary::TYPE::Double:
            case Dictionary::TYPE::String: {
                out << scalar_to_yaml(d);
                break;
            }
            case Dictionary::TYPE::IntArray:
            case Dictionary::TYPE::DoubleArray:
            case Dictionary::TYPE::StringArray:
            case Dictionary::TYPE::BoolArray:
            case Dictionary::TYPE::ObjectArray: {
                // Emit each element on its own "- " line
                if (d.size() == 0) {
                    out << "[]";
                    break;
                }
                for (int i = 0; i < d.size(); ++i) {
                    const Dictionary &el = d.at(i);
                    indent_spaces(indent);
                    out << "- ";
                    bool el_is_simple = (el.type() == Dictionary::TYPE::Null || el.type() == Dictionary::TYPE::Boolean ||
                                         el.type() == Dictionary::TYPE::Integer || el.type() == Dictionary::TYPE::Double ||
                                         el.type() == Dictionary::TYPE::String);
                    if (el_is_simple) {
                        out << scalar_to_yaml(el) << '\n';
                    } else {
                        out << '\n';
                        emit(el, indent + 2);
                    }
                }
                break;
            }
            case Dictionary::TYPE::Object: {
                if (d.empty()) {
                    out << "{}";
                    break;
                }
                // Emit each key: value pair on its own line. If value is
                // scalar we put it inline 'key: value', otherwise newline
                // and nested block.
                auto items = d.items();
                for (auto const &p : items) {
                    indent_spaces(indent);
                    out << p.first << ":";
                    const Dictionary &val = p.second;
                    bool val_is_simple = (val.type() == Dictionary::TYPE::Null || val.type() == Dictionary::TYPE::Boolean ||
                                         val.type() == Dictionary::TYPE::Integer || val.type() == Dictionary::TYPE::Double ||
                                         val.type() == Dictionary::TYPE::String);
                    if (val_is_simple) {
                        out << " " << scalar_to_yaml(val) << '\n';
                    } else {
                        out << '\n';
                        emit(val, indent + 2);
                    }
                }
                break;
            }
            default:
                out << "null";
                break;
        }
    };

    emit = [&](const Dictionary &d, int indent) {
        // Reuse inline/block emitter
        emit_inline_or_block(d, indent, true);
    };

    // Top-level handling: if top is a mapped object, emit keys without
    // leading '-' markers. Otherwise if it's an array, start with list
    // entries. Scalars are emitted as a single value line.
    if (dict.type() == Dictionary::TYPE::Object) {
        emit(dict, 0);
    } else if (dict.isArrayObject()) {
        // arrays at top-level: reuse array logic by wrapping in a pseudo
        // container emission at indent 0
        emit(dict, 0);
    } else {
        // scalar at top-level
        out << scalar_to_yaml(dict) << '\n';
    }

    std::string res = out.str();
    if (!res.empty() && res.back() != '\n') res.push_back('\n');
    return res;
}

} // namespace ps
