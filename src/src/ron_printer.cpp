#include <ps/dictionary.h>
#include <sstream>
#include <string>
#include <functional>
#include <cctype>

namespace ps {

static std::string escape_string_ron(const std::string& s) {
    std::ostringstream ss;
    ss << '"';
    for (char c : s) {
        if (c == '"' || c == '\\') {
            ss << '\\' << c;
        } else if (c == '\n') {
            ss << "\\n";
        } else if (c == '\t') {
            ss << "\\t";
        } else {
            ss << c;
        }
    }
    ss << '"';
    return ss.str();
}

std::string dump_ron(const Dictionary& d) {
    std::ostringstream out;

    std::function<void(const Dictionary&, int)> emit;
    auto indent_spaces = [&](int n) {
        for (int i = 0; i < n; ++i) out.put(' ');
    };

    auto is_simple = [](const Dictionary& v) {
        switch (v.type()) {
            case Dictionary::TYPE::Null:
            case Dictionary::TYPE::Boolean:
            case Dictionary::TYPE::Integer:
            case Dictionary::TYPE::Double:
            case Dictionary::TYPE::String:
                return true;
            default:
                return false;
        }
    };

    emit = [&](const Dictionary& val, int indent) {
        switch (val.type()) {
            case Dictionary::TYPE::Null:
                out << "null";
                break;
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
                out << escape_string_ron(val.asString());
                break;
            case Dictionary::TYPE::IntArray:
            case Dictionary::TYPE::DoubleArray:
            case Dictionary::TYPE::StringArray:
            case Dictionary::TYPE::BoolArray:
            case Dictionary::TYPE::ObjectArray: {
                if (val.size() == 0) {
                    out << "[]";
                    break;
                }

                bool all_simple = true;
                for (int i = 0; i < val.size(); ++i)
                    if (!is_simple(val.at(i))) {
                        all_simple = false;
                        break;
                    }

                // Inline small/simple arrays, otherwise use multi-line style
                if (all_simple && val.size() <= 6) {
                    out << '[';
                    for (int i = 0; i < val.size(); ++i) {
                        if (i) out << ", ";
                        emit(val.at(i), 0);
                    }
                    out << ']';
                } else {
                    out << '[' << '\n';
                    for (int i = 0; i < val.size(); ++i) {
                        indent_spaces(indent + 2);
                        emit(val.at(i), indent + 2);
                        if (i + 1 < val.size()) out << ',';
                        out << '\n';
                    }
                    indent_spaces(indent);
                    out << ']';
                }
                break;
            }
            case Dictionary::TYPE::Object: {
                if (val.empty()) {
                    out << "{}";
                    break;
                }

                out << '{' << '\n';
                auto items = val.items();
                for (size_t idx = 0; idx < items.size(); ++idx) {
                    const auto& p = items[idx];
                    indent_spaces(indent + 2);

                    // Quote key if it contains characters not allowed in unquoted identifiers
                    // Allowed: alphanumeric, underscore, dollar sign
                    bool needs_quoting = false;
                    if (p.first.empty()) {
                        needs_quoting = true;
                    } else {
                        for (char c : p.first) {
                            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' ||
                                  c == '$')) {
                                needs_quoting = true;
                                break;
                            }
                        }
                    }

                    if (needs_quoting) {
                        out << escape_string_ron(p.first);
                    } else {
                        out << p.first;
                    }
                    out << ": ";

                    const Dictionary& v = p.second;
                    if (is_simple(v)) {
                        emit(v, indent + 2);
                    } else {
                        // complex value on following lines
                        emit(v, indent + 2);
                    }
                    if (idx + 1 < items.size()) out << ',';
                    out << '\n';
                }
                indent_spaces(indent);
                out << '}';
                break;
            }
            default:
                out << "null";
                break;
        }
    };

    emit(d, 0);
    out << '\n';
    return out.str();
}

}  // namespace ps
