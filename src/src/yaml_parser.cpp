#include <ps/yaml.h>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace ps {

namespace {
    struct YamlParser {
        const std::string& s;
        size_t i = 0;
        int current_indent = 0;

        YamlParser(const std::string& str) : s(str) {}

        char peek() const { return i < s.size() ? s[i] : '\0'; }
        char get() { return i < s.size() ? s[i++] : '\0'; }

        void skip_ws_inline() {
            // Skip spaces and tabs, but not newlines
            while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
                ++i;
            }
        }

        void skip_ws() {
            // Skip all whitespace including newlines
            while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
                ++i;
            }
        }

        void skip_to_eol() {
            while (i < s.size() && s[i] != '\n') {
                ++i;
            }
            if (i < s.size() && s[i] == '\n') {
                ++i;
            }
        }

        int get_indent() {
            size_t start = i;
            int indent = 0;
            while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
                if (s[i] == ' ') indent++;
                else indent += 2; // treat tab as 2 spaces
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
                get(); // consume opening quote
                std::string out;
                while (true) {
                    char c = get();
                    if (c == '\0') throw std::runtime_error("unterminated quoted string in YAML");
                    if (c == '"') break;
                    if (c == '\\') {
                        char e = get();
                        if (e == 'n') out.push_back('\n');
                        else if (e == 't') out.push_back('\t');
                        else if (e == 'r') out.push_back('\r');
                        else out.push_back(e);
                    } else {
                        out.push_back(c);
                    }
                }
                return out;
            } else if (peek() == '\'') {
                // Single-quoted string
                get(); // consume opening quote
                std::string out;
                while (true) {
                    char c = get();
                    if (c == '\0') throw std::runtime_error("unterminated quoted string in YAML");
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
            if (str.empty()) {
                Dictionary d;
                d = std::string("");
                return d;
            }

            // Check for null
            if (str == "null" || str == "~" || str == "Null" || str == "NULL") {
                return Dictionary::null();
            }

            // Check for boolean
            if (str == "true" || str == "True" || str == "TRUE") {
                Dictionary d;
                d = true;
                return d;
            }
            if (str == "false" || str == "False" || str == "FALSE") {
                Dictionary d;
                d = false;
                return d;
            }

            // Try to parse as number
            std::istringstream ss(str);
            if (str.find('.') != std::string::npos || str.find('e') != std::string::npos || str.find('E') != std::string::npos) {
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
            d = str;
            return d;
        }

        Dictionary parse_array(int base_indent) {
            std::vector<Dictionary> out_values;
            bool allInt = true, allDouble = true, allString = true, allBool = true, allObject = true;

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
                    if (s[i] == ' ' || s[i] == '\t') ++i;
                }

                if (peek() != '-') break;
                get(); // consume '-'
                skip_ws_inline();

                Dictionary v = parse_value(indent + 2);
                out_values.emplace_back(v);

                if (!v.isMappedObject()) allObject = false;
                switch (v.type()) {
                    case Dictionary::Integer:
                        allDouble = false; allString = false; allBool = false; allObject = false;
                        break;
                    case Dictionary::Double:
                        allInt = false; allString = false; allBool = false; allObject = false;
                        break;
                    case Dictionary::String:
                        allInt = false; allDouble = false; allBool = false; allObject = false;
                        break;
                    case Dictionary::Boolean:
                        allInt = false; allDouble = false; allString = false; allObject = false;
                        break;
                    case Dictionary::Object:
                        allInt = false; allDouble = false; allString = false; allBool = false;
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
            Dictionary d;
            
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
                            return d;
                        }
                        if (s[i] == '#') {
                            skip_to_eol();
                            continue;
                        }
                        
                        // Consume indent
                        i += indent;  // Simply skip forward by the indent amount
                        break;
                    }
                }

                if (i >= s.size()) break;

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
                if (!std::isalpha(static_cast<unsigned char>(key[0]))) {
                    throw std::runtime_error(std::string("YAML parse error: invalid key: keys must start with a letter"));
                }

                if (peek() != ':') {
                    throw std::runtime_error("expected ':' after key in YAML object");
                }
                get(); // consume ':'

                if (d.has(key)) {
                    throw std::runtime_error("duplicate key '" + key + "' in YAML object");
                }

                skip_ws_inline();

                Dictionary value;
                if (peek() == '\n' || peek() == '#' || peek() == '\0') {
                    // Value is on next line(s) - could be nested object or array
                    if (peek() == '#') skip_to_eol();
                    else if (peek() == '\n') get();

                    // Check what's next
                    int next_indent = get_indent();
                    if (next_indent > base_indent) {
                        // Don't consume the indent here - let the recursive parser handle it
                        if (s[i + next_indent] == '-') {
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

                d[key] = value;
            }

            return d;
        }

        Dictionary parse_value(int base_indent = 0) {
            skip_ws_inline();

            // Check for array marker
            if (peek() == '-' && i + 1 < s.size() && std::isspace(static_cast<unsigned char>(s[i + 1]))) {
                return parse_array(base_indent);
            }

            // Check if this is an inline object or scalar
            size_t lookahead = i;
            bool has_colon = false;
            while (lookahead < s.size() && s[lookahead] != '\n') {
                if (s[lookahead] == ':' && lookahead + 1 < s.size() && 
                    (std::isspace(static_cast<unsigned char>(s[lookahead + 1])) || s[lookahead + 1] == '\n')) {
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
