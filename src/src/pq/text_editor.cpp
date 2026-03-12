#include <ps/pq/text_editor.h>
#include <cctype>
#include <stdexcept>

namespace ps {
namespace pq {

namespace {

struct ArrayInfo {
    size_t open;               // byte position of '['
    size_t close;              // byte position of ']'
    bool empty;
    size_t first_element_pos;  // valid if !empty
    size_t last_value_end;     // valid if !empty
    bool has_trailing_comma;
    size_t trailing_comma_pos; // valid if has_trailing_comma
};

// Detect the indentation at a given position by walking backward to the
// previous newline.  Returns the whitespace prefix of that line.
std::string detect_indent(const std::string& text, size_t pos) {
    size_t line_start = pos;
    while (line_start > 0 && text[line_start - 1] != '\n') --line_start;
    std::string indent = text.substr(line_start, pos - line_start);
    for (char c : indent) {
        if (c != ' ' && c != '\t') return "";
    }
    return indent;
}

// A lightweight text scanner that walks JSON (with comments) without
// building a tree.  It understands //, #, /* */ comments, quoted strings,
// and balanced braces/brackets — just enough to follow a path and locate
// an array's brackets.
class TextScanner {
    const std::string& s;
    size_t i = 0;

public:
    explicit TextScanner(const std::string& str) : s(str) {}

    char peek() const { return i < s.size() ? s[i] : '\0'; }

    void skip_ws() {
        while (i < s.size()) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            if (std::isspace(c)) { ++i; continue; }
            // line comment //
            if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
                i += 2;
                while (i < s.size() && s[i] != '\n') ++i;
                continue;
            }
            // line comment #
            if (c == '#') {
                ++i;
                while (i < s.size() && s[i] != '\n') ++i;
                continue;
            }
            // block comment /* ... */
            if (c == '/' && i + 1 < s.size() && s[i + 1] == '*') {
                i += 2;
                while (i < s.size()) {
                    if (s[i] == '*' && i + 1 < s.size() && s[i + 1] == '/') {
                        i += 2;
                        break;
                    }
                    ++i;
                }
                continue;
            }
            break;
        }
    }

    void skip_string() {
        if (i >= s.size() || s[i] != '"')
            throw std::runtime_error("TextEditor: expected '\"'");
        ++i;
        while (i < s.size()) {
            if (s[i] == '\\') { i += 2; continue; }
            if (s[i] == '"') { ++i; return; }
            ++i;
        }
        throw std::runtime_error("TextEditor: unterminated string");
    }

    std::string read_string() {
        if (i >= s.size() || s[i] != '"')
            throw std::runtime_error("TextEditor: expected '\"'");
        ++i;
        std::string result;
        while (i < s.size()) {
            if (s[i] == '\\') {
                ++i;
                if (i < s.size()) { result += s[i]; ++i; }
                continue;
            }
            if (s[i] == '"') { ++i; return result; }
            result += s[i]; ++i;
        }
        throw std::runtime_error("TextEditor: unterminated string");
    }

    void skip_value() {
        skip_ws();
        char c = peek();
        if (c == '"')  { skip_string(); return; }
        if (c == '{')  { skip_object(); return; }
        if (c == '[')  { skip_array(); return; }
        // number, bool, null — advance until a structural character
        while (i < s.size()) {
            c = s[i];
            if (c == ',' || c == '}' || c == ']' ||
                std::isspace(static_cast<unsigned char>(c)) ||
                c == '/' || c == '#')
                break;
            ++i;
        }
    }

    void skip_object() {
        if (i >= s.size() || s[i] != '{')
            throw std::runtime_error("TextEditor: expected '{'");
        ++i;
        skip_ws();
        if (peek() == '}') { ++i; return; }
        while (true) {
            skip_ws();
            skip_string(); // key
            skip_ws();
            if (i >= s.size() || s[i] != ':')
                throw std::runtime_error("TextEditor: expected ':'");
            ++i;
            skip_value();
            skip_ws();
            if (peek() == ',') { ++i; continue; }
            if (peek() == '}') { ++i; return; }
            throw std::runtime_error("TextEditor: expected ',' or '}'");
        }
    }

    void skip_array() {
        if (i >= s.size() || s[i] != '[')
            throw std::runtime_error("TextEditor: expected '['");
        ++i;
        skip_ws();
        if (peek() == ']') { ++i; return; }
        while (true) {
            skip_value();
            skip_ws();
            if (peek() == ',') { ++i; continue; }
            if (peek() == ']') { ++i; return; }
            throw std::runtime_error("TextEditor: expected ',' or ']'");
        }
    }

    // Navigate into an object to position at the value for `key`.
    // Scanner must be at '{'.  After return scanner is at the start of the value.
    void enter_object_key(const std::string& key) {
        if (i >= s.size() || s[i] != '{')
            throw std::runtime_error("TextEditor: expected '{'");
        ++i;
        while (true) {
            skip_ws();
            if (peek() == '}')
                throw std::out_of_range("TextEditor: key '" + key + "' not found");
            std::string k = read_string();
            skip_ws();
            if (i >= s.size() || s[i] != ':')
                throw std::runtime_error("TextEditor: expected ':'");
            ++i;
            skip_ws();
            if (k == key) return; // positioned at value
            skip_value();
            skip_ws();
            if (peek() == ',') { ++i; }
        }
    }

    // Navigate into an array to position at element `index`.
    // Scanner must be at '['.  After return scanner is at the start of the element.
    void enter_array_index(int index) {
        if (i >= s.size() || s[i] != '[')
            throw std::runtime_error("TextEditor: expected '['");
        ++i;
        for (int idx = 0; idx <= index; ++idx) {
            skip_ws();
            if (peek() == ']')
                throw std::out_of_range("TextEditor: array index " +
                                        std::to_string(index) + " out of range");
            if (idx == index) return; // positioned at element
            skip_value();
            skip_ws();
            if (peek() == ',') { ++i; }
        }
        throw std::out_of_range("TextEditor: array index " +
                                std::to_string(index) + " out of range");
    }

    // Follow the path tokens and return info about the target array.
    ArrayInfo findArrayAtPath(const std::vector<PathToken>& tokens) {
        skip_ws();
        for (const auto& tok : tokens) {
            skip_ws();
            if (tok.isKey()) {
                enter_object_key(tok.asKey());
            } else if (tok.isIndex()) {
                enter_array_index(tok.asIndex());
            } else {
                throw std::invalid_argument(
                    "TextEditor: wildcards not supported for text editing");
            }
        }
        skip_ws();
        if (peek() != '[')
            throw std::runtime_error("TextEditor: path does not point to an array");

        ArrayInfo info{};
        info.open = i;
        info.has_trailing_comma = false;
        ++i; // skip '['

        skip_ws();
        if (peek() == ']') {
            info.close = i;
            info.empty = true;
            return info;
        }

        info.empty = false;
        info.first_element_pos = i;

        while (true) {
            skip_value();
            info.last_value_end = i;
            skip_ws();
            if (peek() == ',') {
                info.has_trailing_comma = true;
                info.trailing_comma_pos = i;
                ++i;
                skip_ws();
                if (peek() == ']') {
                    info.close = i;
                    return info;
                }
                info.has_trailing_comma = false;
                continue;
            }
            if (peek() == ']') {
                info.close = i;
                return info;
            }
            throw std::runtime_error("TextEditor: expected ',' or ']' in array");
        }
    }
};

} // anonymous namespace

std::string TextEditor::prependToArray(const std::string& text,
                                        const std::vector<PathToken>& path,
                                        const std::string& valueText) {
    TextScanner scanner(text);
    auto info = scanner.findArrayAtPath(path);

    if (info.empty) {
        std::string outer = detect_indent(text, info.close);
        std::string inner = outer.empty() ? "    " : outer + "    ";
        return text.substr(0, info.open + 1)
             + "\n" + inner + valueText + "\n" + outer
             + text.substr(info.close);
    }

    std::string indent = detect_indent(text, info.first_element_pos);
    if (indent.empty()) {
        // Inline array — insert with a space
        return text.substr(0, info.open + 1)
             + valueText + ", "
             + text.substr(info.open + 1);
    }

    return text.substr(0, info.open + 1)
         + "\n" + indent + valueText + ","
         + text.substr(info.open + 1);
}

std::string TextEditor::appendToArray(const std::string& text,
                                       const std::vector<PathToken>& path,
                                       const std::string& valueText) {
    TextScanner scanner(text);
    auto info = scanner.findArrayAtPath(path);

    if (info.empty) {
        std::string outer = detect_indent(text, info.close);
        std::string inner = outer.empty() ? "    " : outer + "    ";
        return text.substr(0, info.open + 1)
             + "\n" + inner + valueText + "\n" + outer
             + text.substr(info.close);
    }

    std::string indent = detect_indent(text, info.first_element_pos);

    if (indent.empty()) {
        // Inline array — append with a comma
        if (info.has_trailing_comma) {
            return text.substr(0, info.trailing_comma_pos + 1)
                 + " " + valueText + ","
                 + text.substr(info.trailing_comma_pos + 1);
        }
        return text.substr(0, info.last_value_end)
             + ", " + valueText
             + text.substr(info.last_value_end);
    }

    if (info.has_trailing_comma) {
        // Preserve trailing-comma style: insert after the comma
        return text.substr(0, info.trailing_comma_pos + 1)
             + "\n" + indent + valueText + ","
             + text.substr(info.trailing_comma_pos + 1);
    }
    // No trailing comma: add one after last value
    return text.substr(0, info.last_value_end)
         + ",\n" + indent + valueText
         + text.substr(info.last_value_end);
}

} // namespace pq
} // namespace ps
