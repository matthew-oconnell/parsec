#include <ps/yaml.h>
#include <cctype>
#include <map>
#include <sstream>
#include <stdexcept>

namespace ps {

namespace {
    struct YamlParseError : public std::runtime_error {
        size_t line, col;
        YamlParseError(const std::string& msg, size_t l, size_t c)
            : std::runtime_error(msg), line(l), col(c) {}
    };

    struct YamlParser {
        const std::string& s;
        size_t i = 0;
        size_t line = 1;
        size_t col = 1;
        int current_indent = 0;

        std::map<std::string, Dictionary> anchors;

        YamlParser(const std::string& str) : s(str) {}

        char peek() const { return i < s.size() ? s[i] : '\0'; }

        char get() {
            if (i >= s.size()) return '\0';
            char c = s[i++];
            if (c == '\n') {
                ++line;
                col = 1;
            } else {
                ++col;
            }
            return c;
        }

        std::string format_error(const std::string& base, size_t err_line, size_t err_col) const {
            // find start of error line
            size_t pos = 0;
            size_t cur = 1;
            while (cur < err_line && pos < s.size()) {
                if (s[pos] == '\n') ++cur;
                ++pos;
            }
            size_t line_start = pos;
            size_t line_end = pos;
            while (line_end < s.size() && s[line_end] != '\n') ++line_end;
            std::string line_text = s.substr(line_start, line_end - line_start);

            // build caret (position columns as bytes)
            size_t caret_pos = err_col > 0 ? err_col - 1 : 0;
            if (caret_pos > line_text.size()) caret_pos = line_text.size();
            std::string caret(caret_pos, ' ');
            caret.push_back('^');

            std::ostringstream ss;
            ss << base << " (line " << err_line << ", column " << err_col << ")\n";
            ss << line_text << "\n" << caret;
            return ss.str();
        }

        void skip_ws_inline() {
            // Skip spaces and tabs, but not newlines
            while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
                get();
            }
        }

        void skip_ws() {
            // Skip all whitespace including newlines
            while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
                get();
            }
        }

        void skip_to_eol() {
            while (i < s.size() && s[i] != '\n') {
                get();
            }
            if (i < s.size() && s[i] == '\n') {
                get();
            }
        }

        int get_indent() {
            size_t start = i;
            int indent = 0;
            while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
                if (s[i] == ' ')
                    indent++;
                else
                    indent += 2;  // treat tab as 2 spaces
                ++i;
            }
            // If line is empty or comment, ignore indent
            if (i >= s.size() || s[i] == '\n' || s[i] == '#') {
                i = start;
                return -1;
            }
            i = start;
            return indent;
        }

        std::string parse_string_value() {
            skip_ws_inline();

            if (peek() == '"') {
                // Quoted string
                size_t quote_line = line, quote_col = col;
                get();  // consume opening quote
                std::string out;
                while (true) {
                    char c = get();
                    if (c == '\0') {
                        throw YamlParseError(
                                    format_error("YAML parse error: unterminated quoted string",
                                                 quote_line,
                                                 quote_col),
                                    quote_line,
                                    quote_col);
                    }
                    if (c == '"') break;
                    if (c == '\\') {
                        char e = get();
                        if (e == 'n')
                            out.push_back('\n');
                        else if (e == 't')
                            out.push_back('\t');
                        else if (e == 'r')
                            out.push_back('\r');
                        else
                            out.push_back(e);
                    } else {
                        out.push_back(c);
                    }
                }
                return out;
            } else if (peek() == '\'') {
                // Single-quoted string
                size_t quote_line = line, quote_col = col;
                get();  // consume opening quote
                std::string out;
                while (true) {
                    char c = get();
                    if (c == '\0') {
                        throw YamlParseError(
                                    format_error("YAML parse error: unterminated quoted string",
                                                 quote_line,
                                                 quote_col),
                                    quote_line,
                                    quote_col);
                    }
                    if (c == '\'') {
                        // Check for escaped single quote
                        if (peek() == '\'') {
                            out.push_back('\'');
                            get();
                        } else {
                            break;
                        }
                    } else {
                        out.push_back(c);
                    }
                }
                return out;
            } else {
                // Unquoted string - read until newline or comment
                std::string out;
                while (i < s.size() && s[i] != '\n' && s[i] != '#') {
                    out.push_back(s[i]);
                    ++i;
                }
                // Trim trailing whitespace
                while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back()))) {
                    out.pop_back();
                }
                return out;
            }
        }

        Dictionary parse_scalar(const std::string& str) {
            auto trim = [](const std::string& in) {
                size_t start = 0;
                while (start < in.size() && std::isspace(static_cast<unsigned char>(in[start]))) {
                    ++start;
                }
                size_t end = in.size();
                while (end > start && std::isspace(static_cast<unsigned char>(in[end - 1]))) {
                    --end;
                }
                return in.substr(start, end - start);
            };

            const std::string t = trim(str);

            // Alias for an anchored node (used in scalars and flow arrays like [*a, *b])
            if (t.size() >= 2 && t.front() == '*') {
                std::string name = t.substr(1);
                bool ok = !name.empty();
                for (char c : name) {
                    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')) {
                        ok = false;
                        break;
                    }
                }
                if (ok) {
                    auto it = anchors.find(name);
                    if (it == anchors.end()) {
                        throw std::runtime_error("YAML parse error: unknown anchor '*" + name +
                                                 "'");
                    }
                    return it->second;
                }
            }

            if (t.empty()) {
                Dictionary d;
                d = std::string("");
                return d;
            }

            // Flow-style empty containers as scalars: '{}' and '[]'
            if (t == "{}") {
                return Dictionary();
            }
            if (t == "[]") {
                Dictionary res;
                res = std::vector<Dictionary>{};
                return res;
            }

            // Flow-style sequences like: [1, 2, "three"]
            if (t.size() >= 2 && t.front() == '[' && t.back() == ']') {
                const std::string inner = trim(t.substr(1, t.size() - 2));
                std::vector<Dictionary> out_values;
                bool allInt = true, allDouble = true, allString = true, allBool = true,
                     allObject = true;

                if (!inner.empty()) {
                    std::vector<std::string> tokens;
                    std::string cur;
                    bool in_double = false;
                    bool in_single = false;
                    bool escape = false;

                    for (size_t k = 0; k < inner.size(); ++k) {
                        const char c = inner[k];
                        if (in_double) {
                            cur.push_back(c);
                            if (escape) {
                                escape = false;
                            } else if (c == '\\') {
                                escape = true;
                            } else if (c == '"') {
                                in_double = false;
                            }
                            continue;
                        }
                        if (in_single) {
                            cur.push_back(c);
                            if (c == '\'') {
                                in_single = false;
                            }
                            continue;
                        }

                        if (c == '"') {
                            in_double = true;
                            cur.push_back(c);
                            continue;
                        }
                        if (c == '\'') {
                            in_single = true;
                            cur.push_back(c);
                            continue;
                        }

                        if (c == ',') {
                            tokens.push_back(cur);
                            cur.clear();
                            continue;
                        }

                        cur.push_back(c);
                    }
                    tokens.push_back(cur);

                    auto unescape_double_quoted = [](const std::string& q) {
                        // q includes surrounding quotes
                        std::string out;
                        for (size_t p = 1; p + 1 < q.size(); ++p) {
                            char c = q[p];
                            if (c == '\\' && p + 1 < q.size() - 1) {
                                char e = q[++p];
                                if (e == 'n')
                                    out.push_back('\n');
                                else if (e == 't')
                                    out.push_back('\t');
                                else if (e == 'r')
                                    out.push_back('\r');
                                else
                                    out.push_back(e);
                            } else {
                                out.push_back(c);
                            }
                        }
                        return out;
                    };

                    auto unescape_single_quoted = [](const std::string& q) {
                        // q includes surrounding single quotes; YAML escapes a single quote as ''
                        std::string out;
                        for (size_t p = 1; p + 1 < q.size(); ++p) {
                            char c = q[p];
                            if (c == '\'' && p + 1 < q.size() - 1 && q[p + 1] == '\'') {
                                out.push_back('\'');
                                ++p;
                            } else {
                                out.push_back(c);
                            }
                        }
                        return out;
                    };

                    for (auto& tok_raw : tokens) {
                        const std::string tok = trim(tok_raw);
                        if (tok.empty()) continue;

                        Dictionary v;
                        if (tok.size() >= 2 && tok.front() == '"' && tok.back() == '"') {
                            v = unescape_double_quoted(tok);
                        } else if (tok.size() >= 2 && tok.front() == '\'' && tok.back() == '\'') {
                            v = unescape_single_quoted(tok);
                        } else {
                            v = parse_scalar(tok);
                        }

                        out_values.emplace_back(v);

                        if (!v.isMappedObject()) allObject = false;
                        switch (v.type()) {
                            case Dictionary::Integer:
                                allDouble = false;
                                allString = false;
                                allBool = false;
                                allObject = false;
                                break;
                            case Dictionary::Double:
                                allInt = false;
                                allString = false;
                                allBool = false;
                                allObject = false;
                                break;
                            case Dictionary::String:
                                allInt = false;
                                allDouble = false;
                                allBool = false;
                                allObject = false;
                                break;
                            case Dictionary::Boolean:
                                allInt = false;
                                allDouble = false;
                                allString = false;
                                allObject = false;
                                break;
                            case Dictionary::Object:
                                allInt = false;
                                allDouble = false;
                                allString = false;
                                allBool = false;
                                break;
                            default:
                                allInt = allDouble = allString = allBool = allObject = false;
                                break;
                        }
                    }
                }

                Dictionary res;
                if (out_values.empty()) {
                    res = std::vector<Dictionary>{};
                    return res;
                }

                if (allInt) {
                    std::vector<int> vals;
                    vals.reserve(out_values.size());
                    for (auto& v : out_values) vals.push_back(static_cast<int>(v.asInt()));
                    res = vals;
                } else if (allDouble) {
                    std::vector<double> vals;
                    vals.reserve(out_values.size());
                    for (auto& v : out_values) vals.push_back(v.asDouble());
                    res = vals;
                } else if (allString) {
                    std::vector<std::string> vals;
                    vals.reserve(out_values.size());
                    for (auto& v : out_values) vals.push_back(v.asString());
                    res = vals;
                } else if (allBool) {
                    std::vector<bool> vals;
                    vals.reserve(out_values.size());
                    for (auto& v : out_values) vals.push_back(v.asBool());
                    res = vals;
                } else {
                    res = out_values;
                }
                return res;
            }

            // Check for null
            if (t == "null" || t == "~" || t == "Null" || t == "NULL") {
                return Dictionary::null();
            }

            // Check for boolean
            if (t == "true" || t == "True" || t == "TRUE") {
                Dictionary d;
                d = true;
                return d;
            }
            if (t == "false" || t == "False" || t == "FALSE") {
                Dictionary d;
                d = false;
                return d;
            }

            // Try to parse as number
            std::istringstream ss(t);
            if (t.find('.') != std::string::npos || t.find('e') != std::string::npos ||
                t.find('E') != std::string::npos) {
                double dval;
                ss >> dval;
                if (!ss.fail() && ss.eof()) {
                    Dictionary d;
                    d = dval;
                    return d;
                }
            } else {
                // Try integer
                int64_t v;
                ss >> v;
                if (!ss.fail() && ss.eof()) {
                    Dictionary d;
                    d = v;
                    return d;
                }
            }

            // Default to string
            Dictionary d;
            d = t;
            return d;
        }

        bool is_anchor_name_char(char c) const {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-';
        }

        std::string parse_anchor_name(char prefix) {
            // prefix is '&' or '*', and current input is positioned at that prefix
            const size_t start_line = line;
            const size_t start_col = col;
            get();  // consume prefix

            std::string name;
            while (i < s.size() && is_anchor_name_char(peek())) {
                name.push_back(get());
            }

            if (name.empty()) {
                throw YamlParseError(format_error(std::string("YAML parse error: expected anchor "
                                                              "name after '") +
                                                              prefix + "'",
                                                  start_line,
                                                  start_col),
                                     start_line,
                                     start_col);
            }

            // YAML token boundary: require delimiter after the name.
            const char next = peek();
            if (next != '\0' && next != '\n' && next != '#' && next != ',' && next != ']' &&
                next != '}' && !std::isspace(static_cast<unsigned char>(next))) {
                throw YamlParseError(format_error(std::string("YAML parse error: invalid anchor "
                                                              "reference '") +
                                                              prefix + name + "'",
                                                  start_line,
                                                  start_col),
                                     start_line,
                                     start_col);
            }

            return name;
        }

        Dictionary parse_array(int base_indent) {
            std::vector<Dictionary> out_values;
            bool allInt = true, allDouble = true, allString = true, allBool = true,
                 allObject = true;

            while (i < s.size()) {
                // Skip empty lines and comments
                while (i < s.size()) {
                    int indent = get_indent();
                    if (indent == -1) {
                        skip_to_eol();
                        continue;
                    }
                    if (indent < base_indent) {
                        // End of array
                        goto array_done;
                    }
                    if (s[i] == '#') {
                        skip_to_eol();
                        continue;
                    }
                    break;
                }

                if (i >= s.size()) break;

                int indent = get_indent();
                if (indent < base_indent) break;

                // Consume indent
                for (int j = 0; j < indent; ++j) {
                    get();  // Use get() to properly track line/col
                }

                if (peek() != '-') break;
                get();  // consume '-'
                skip_ws_inline();

                Dictionary v = parse_value(indent + 2);
                out_values.emplace_back(v);

                if (!v.isMappedObject()) allObject = false;
                switch (v.type()) {
                    case Dictionary::Integer:
                        allDouble = false;
                        allString = false;
                        allBool = false;
                        allObject = false;
                        break;
                    case Dictionary::Double:
                        allInt = false;
                        allString = false;
                        allBool = false;
                        allObject = false;
                        break;
                    case Dictionary::String:
                        allInt = false;
                        allDouble = false;
                        allBool = false;
                        allObject = false;
                        break;
                    case Dictionary::Boolean:
                        allInt = false;
                        allDouble = false;
                        allString = false;
                        allObject = false;
                        break;
                    case Dictionary::Object:
                        allInt = false;
                        allDouble = false;
                        allString = false;
                        allBool = false;
                        break;
                    default:
                        allInt = allDouble = allString = allBool = allObject = false;
                        break;
                }
            }

        array_done:
            Dictionary res;
            if (out_values.empty()) {
                res = std::vector<Dictionary>{};
                return res;
            }

            if (allInt) {
                std::vector<int> vals;
                for (auto& v : out_values) vals.push_back(static_cast<int>(v.asInt()));
                res = vals;
            } else if (allDouble) {
                std::vector<double> vals;
                for (auto& v : out_values) vals.push_back(v.asDouble());
                res = vals;
            } else if (allString) {
                std::vector<std::string> vals;
                for (auto& v : out_values) vals.push_back(v.asString());
                res = vals;
            } else if (allBool) {
                std::vector<bool> vals;
                for (auto& v : out_values) vals.push_back(v.asBool());
                res = vals;
            } else {
                res = out_values;
            }
            return res;
        }

        Dictionary parse_object(int base_indent) {
            Dictionary explicit_entries;
            std::vector<Dictionary> merge_sources;

            auto finalize = [&]() {
                Dictionary out;

                // Apply merge sources first (later sources override earlier ones)
                for (const auto& src : merge_sources) {
                    if (!src.isMappedObject()) {
                        continue;
                    }
                    for (const auto& k : src.keys()) {
                        out[k] = src.at(k);
                    }
                }

                // Explicit keys always override merged keys
                for (const auto& k : explicit_entries.keys()) {
                    out[k] = explicit_entries.at(k);
                }
                return out;
            };

            while (i < s.size()) {
                // Skip inline whitespace if we're starting inline (after '-' or mid-line)
                // Otherwise handle line-based indentation
                bool is_inline = (i > 0 && s[i - 1] != '\n');

                if (is_inline) {
                    skip_ws_inline();
                } else {
                    // Skip empty lines and comments, handle indentation
                    while (i < s.size()) {
                        int indent = get_indent();
                        if (indent == -1) {
                            skip_to_eol();
                            continue;
                        }
                        if (indent < base_indent) {
                            return finalize();
                        }
                        if (s[i] == '#') {
                            skip_to_eol();
                            continue;
                        }

                        // Consume indent
                        for (int j = 0; j < indent; ++j) {
                            get();  // Use get() to properly track line/col
                        }
                        break;
                    }
                }

                if (i >= s.size()) break;

                // If we encounter a '-' at this indent level, we're done with this object
                // (we're in an array and this is the next array element)
                if (peek() == '-' && i + 1 < s.size() &&
                    std::isspace(static_cast<unsigned char>(s[i + 1]))) {
                    return finalize();
                }

                // Parse key
                std::string key;
                while (i < s.size() && s[i] != ':' && s[i] != '\n') {
                    key.push_back(s[i]);
                    ++i;
                }

                // Trim trailing whitespace from key
                while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) {
                    key.pop_back();
                }

                if (key.empty()) {
                    skip_to_eol();
                    continue;
                }

                // Keys must start with an ASCII letter
                if (key != "<<" && !std::isalpha(static_cast<unsigned char>(key[0]))) {
                    // Calculate the position of the start of the key
                    size_t key_pos = i - key.length();
                    size_t key_line = 1, key_col = 1;
                    for (size_t p = 0; p < key_pos && p < s.size(); ++p) {
                        if (s[p] == '\n') {
                            ++key_line;
                            key_col = 1;
                        } else {
                            ++key_col;
                        }
                    }
                    throw YamlParseError(
                                format_error("YAML parse error: invalid key '" + key +
                                                         "': keys must start with a letter",
                                             key_line,
                                             key_col),
                                key_line,
                                key_col);
                }

                if (peek() != ':') {
                    throw YamlParseError(format_error("YAML parse error: expected ':' after key '" +
                                                                  key + "'",
                                                      line,
                                                      col),
                                         line,
                                         col);
                }
                get();  // consume ':'

                if (key != "<<" && explicit_entries.has(key)) {
                    throw YamlParseError(
                                format_error("YAML parse error: duplicate key '" + key + "'",
                                             line,
                                             col),
                                line,
                                col);
                }

                skip_ws_inline();

                Dictionary value;
                if (peek() == '\n' || peek() == '#' || peek() == '\0') {
                    // Value is on next line(s) - could be nested object or array
                    if (peek() == '#')
                        skip_to_eol();
                    else if (peek() == '\n')
                        get();

                    // Check what's next
                    int next_indent = get_indent();
                    if (next_indent > base_indent) {
                        // Don't consume the indent here - let the recursive parser handle it
                        char next_char = (i + next_indent < s.size()) ? s[i + next_indent] : '\0';
                        if (next_char == '-') {
                            // It's an array
                            value = parse_array(next_indent);
                        } else {
                            // It's a nested object
                            value = parse_object(next_indent);
                        }
                    } else {
                        // Empty value
                        value = Dictionary::null();
                    }
                } else {
                    // Value is on same line
                    value = parse_value(base_indent);
                    // Skip any trailing comment
                    if (peek() == '#') {
                        skip_to_eol();
                    }
                }

                if (key == "<<") {
                    if (value.isMappedObject() || value.type() == Dictionary::ObjectArray) {
                        for (const auto& obj : value.asObjects()) {
                            if (!obj.isMappedObject()) {
                                throw YamlParseError(format_error("YAML parse error: merge key "
                                                                  "'<<' must reference a mapping",
                                                                  line,
                                                                  col),
                                                     line,
                                                     col);
                            }
                            merge_sources.push_back(obj);
                        }
                    } else {
                        throw YamlParseError(format_error("YAML parse error: merge key '<<' must "
                                                          "reference a mapping or list of mappings",
                                                          line,
                                                          col),
                                             line,
                                             col);
                    }
                } else {
                    explicit_entries[key] = value;
                }
            }

            return finalize();
        }

        Dictionary parse_value_no_anchor(int base_indent = 0) {
            skip_ws_inline();

            // If we're at a newline, the value is on the next line(s)
            if (peek() == '\n' || peek() == '#' || peek() == '\0') {
                if (peek() == '#')
                    skip_to_eol();
                else if (peek() == '\n')
                    get();

                // Check what's next
                int next_indent = get_indent();
                if (next_indent >= base_indent) {
                    // Don't consume the indent - let the recursive parser handle it
                    if (s[i + next_indent] == '-') {
                        return parse_array(next_indent);
                    } else {
                        return parse_object(next_indent);
                    }
                } else {
                    // Empty value
                    return Dictionary::null();
                }
            }

            // Check for array marker
            if (peek() == '-' && i + 1 < s.size() &&
                std::isspace(static_cast<unsigned char>(s[i + 1]))) {
                return parse_array(base_indent);
            }

            // Check if this is an inline object or scalar
            size_t lookahead = i;
            bool has_colon = false;
            while (lookahead < s.size() && s[lookahead] != '\n') {
                if (s[lookahead] == ':' && lookahead + 1 < s.size() &&
                    (std::isspace(static_cast<unsigned char>(s[lookahead + 1])) ||
                     s[lookahead + 1] == '\n')) {
                    has_colon = true;
                    break;
                }
                ++lookahead;
            }

            if (has_colon) {
                return parse_object(base_indent);
            } else {
                std::string str_val = parse_string_value();
                return parse_scalar(str_val);
            }
        }

        Dictionary parse_value(int base_indent = 0) {
            skip_ws_inline();

            if (peek() == '&') {
                const std::string name = parse_anchor_name('&');
                skip_ws_inline();
                Dictionary v = parse_value_no_anchor(base_indent);
                anchors[name] = v;
                return v;
            }

            if (peek() == '*') {
                const size_t ref_line = line;
                const size_t ref_col = col;
                const std::string name = parse_anchor_name('*');
                auto it = anchors.find(name);
                if (it == anchors.end()) {
                    throw YamlParseError(
                                format_error("YAML parse error: unknown anchor '*" + name + "'",
                                             ref_line,
                                             ref_col),
                                ref_line,
                                ref_col);
                }
                return it->second;
            }

            return parse_value_no_anchor(base_indent);
        }

        Dictionary parse() {
            skip_ws();
            if (i >= s.size()) {
                return Dictionary();
            }

            // Check if root is array or object
            int indent = get_indent();
            if (indent == -1) {
                skip_to_eol();
                return parse();
            }

            // Consume indent
            for (int j = 0; j < indent; ++j) {
                if (s[i] == ' ' || s[i] == '\t') ++i;
            }

            if (peek() == '-') {
                return parse_array(0);
            } else {
                return parse_object(0);
            }
        }
    };
}

Dictionary parse_yaml(const std::string& text) {
    YamlParser parser(text);
    return parser.parse();
}

}  // namespace ps
