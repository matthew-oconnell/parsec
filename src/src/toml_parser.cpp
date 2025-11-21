#include <ps/toml.h>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <ctime>
#include <limits>
#include <iomanip>

namespace ps {

namespace {
    struct TomlParser {
        const std::string& s;
        size_t i = 0;
        size_t line = 1;
        size_t column = 1;
        Dictionary root;
        std::vector<std::string> current_table;

        TomlParser(const std::string& str) : s(str) {}

        char peek() const { return i < s.size() ? s[i] : '\0'; }
        char get() {
            if (i < s.size()) {
                char c = s[i++];
                if (c == '\n') {
                    ++line;
                    column = 1;
                } else {
                    ++column;
                }
                return c;
            }
            return '\0';
        }

        void skip_ws_inline() {
            while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
                ++i;
                ++column;
            }
        }

        void skip_ws_and_newlines() {
            while (i < s.size()) {
                if (s[i] == ' ' || s[i] == '\t') {
                    ++i;
                    ++column;
                } else if (s[i] == '\n' || s[i] == '\r') {
                    if (s[i] == '\n') {
                        ++line;
                        column = 1;
                    }
                    ++i;
                } else {
                    break;
                }
            }
        }

        void skip_comment() {
            if (peek() == '#') {
                while (peek() != '\0' && peek() != '\n') {
                    get();
                }
            }
        }

        void skip_ws_and_comments() {
            while (true) {
                skip_ws_inline();
                if (peek() == '#') {
                    skip_comment();
                    if (peek() == '\n') {
                        get();
                    }
                } else if (peek() == '\n' || peek() == '\r') {
                    get();
                } else {
                    break;
                }
            }
        }

        std::string parse_error(const std::string& msg) {
            std::ostringstream ss;
            ss << "TOML parse error: " << msg << " (line " << line << ", column " << column << ")";
            return ss.str();
        }

        bool is_bare_key_char(char c) {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-';
        }

        std::string parse_key() {
            skip_ws_inline();
            if (peek() == '"' || peek() == '\'') {
                return parse_string();
            }
            // Bare key
            std::string key;
            while (is_bare_key_char(peek())) {
                key += get();
            }
            if (key.empty()) {
                throw std::runtime_error(parse_error("expected key"));
            }
            // Keys must start with an ASCII letter
            unsigned char first_ch = static_cast<unsigned char>(key[0]);
            if (!std::isalpha(first_ch)) {
                throw std::runtime_error(parse_error("invalid key: keys must start with a letter"));
            }
            return key;
        }

        std::string parse_string() {
            char quote = peek();
            if (quote != '"' && quote != '\'') {
                throw std::runtime_error(parse_error("expected string"));
            }
            get(); // consume opening quote

            // Check for triple-quoted string
            bool is_triple = false;
            if (peek() == quote && i + 1 < s.size() && s[i + 1] == quote) {
                get();
                get();
                is_triple = true;
                // Skip immediately following newline in triple-quoted strings
                if (quote == '"' && peek() == '\n') {
                    get();
                }
            }

            std::string result;
            while (true) {
                char c = peek();
                if (c == '\0') {
                    throw std::runtime_error(parse_error("unterminated string"));
                }

                if (c == quote) {
                    if (is_triple) {
                        // Check for triple quote
                        if (i + 2 < s.size() && s[i + 1] == quote && s[i + 2] == quote) {
                            get(); get(); get();
                            break;
                        } else {
                            result += get();
                        }
                    } else {
                        get(); // consume closing quote
                        break;
                    }
                } else if (c == '\\' && quote == '"') {
                    // Escape sequences (only in double-quoted strings)
                    get();
                    char e = get();
                    switch (e) {
                        case 'n': result += '\n'; break;
                        case 't': result += '\t'; break;
                        case 'r': result += '\r'; break;
                        case '\\': result += '\\'; break;
                        case '"': result += '"'; break;
                        case 'b': result += '\b'; break;
                        case 'f': result += '\f'; break;
                        default:
                            result += e;
                            break;
                    }
                } else {
                    result += get();
                }
            }
            return result;
        }

        Dictionary parse_number() {
            size_t start = i;
            
            // Handle sign
            bool is_negative = false;
            if (peek() == '+') {
                get();
            } else if (peek() == '-') {
                get();
                is_negative = true;
            }

            // Handle special values
            if (peek() == 'i' || peek() == 'n') {
                std::string word;
                while (std::isalpha(static_cast<unsigned char>(peek()))) {
                    word += get();
                }
                Dictionary d;
                if (word == "inf") {
                    d = is_negative ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
                    return d;
                } else if (word == "nan") {
                    d = std::numeric_limits<double>::quiet_NaN();
                    return d;
                }
            }

            bool is_float = false;
            while (peek() != '\0' && !std::isspace(static_cast<unsigned char>(peek())) && 
                   peek() != ',' && peek() != ']' && peek() != '}' && peek() != '#') {
                if (peek() == '.' || peek() == 'e' || peek() == 'E') {
                    is_float = true;
                }
                get();
            }

            std::string num_str = s.substr(start, i - start);
            // Remove underscores (TOML allows them as separators)
            num_str.erase(std::remove(num_str.begin(), num_str.end(), '_'), num_str.end());

            Dictionary d;
            if (is_float) {
                try {
                    double val = std::stod(num_str);
                    d = val;
                } catch (...) {
                    throw std::runtime_error(parse_error("invalid float: " + num_str));
                }
            } else {
                try {
                    int64_t val = std::stoll(num_str);
                    d = val;
                } catch (...) {
                    throw std::runtime_error(parse_error("invalid integer: " + num_str));
                }
            }
            return d;
        }

        Dictionary parse_boolean() {
            std::string word;
            while (std::isalpha(static_cast<unsigned char>(peek()))) {
                word += get();
            }
            Dictionary d;
            if (word == "true") {
                d = true;
            } else if (word == "false") {
                d = false;
            } else {
                throw std::runtime_error(parse_error("invalid boolean: " + word));
            }
            return d;
        }

        Dictionary parse_datetime() {
            // Simple datetime parsing - store as string for now
            // Full RFC 3339 parsing would be more complex
            size_t start = i;
            while (peek() != '\0' && !std::isspace(static_cast<unsigned char>(peek())) && 
                   peek() != ',' && peek() != ']' && peek() != '}' && peek() != '#') {
                get();
            }
            std::string dt = s.substr(start, i - start);
            Dictionary d;
            d = dt;
            return d;
        }

        Dictionary parse_array() {
            if (get() != '[') {
                throw std::runtime_error(parse_error("expected '['"));
            }

            std::vector<Dictionary> elements;
            skip_ws_and_comments();

            while (peek() != ']') {
                elements.push_back(parse_value());
                skip_ws_and_comments();

                if (peek() == ',') {
                    get();
                    skip_ws_and_comments();
                } else if (peek() != ']') {
                    throw std::runtime_error(parse_error("expected ',' or ']' in array"));
                }
            }

            if (get() != ']') {
                throw std::runtime_error(parse_error("expected ']'"));
            }

            // Convert to typed array if possible
            Dictionary d;
            if (elements.empty()) {
                d = std::vector<Dictionary>();
                return d;
            }

            // Check if all elements are the same type
            bool all_int = true, all_double = true, all_string = true, all_bool = true;
            for (const auto& elem : elements) {
                if (elem.type() != Dictionary::Integer) all_int = false;
                if (elem.type() != Dictionary::Double) all_double = false;
                if (elem.type() != Dictionary::String) all_string = false;
                if (elem.type() != Dictionary::Boolean) all_bool = false;
            }

            if (all_int) {
                std::vector<int> ints;
                for (const auto& elem : elements) {
                    ints.push_back(static_cast<int>(elem.asInt()));
                }
                d = ints;
            } else if (all_double) {
                std::vector<double> doubles;
                for (const auto& elem : elements) {
                    doubles.push_back(elem.asDouble());
                }
                d = doubles;
            } else if (all_string) {
                std::vector<std::string> strings;
                for (const auto& elem : elements) {
                    strings.push_back(elem.asString());
                }
                d = strings;
            } else if (all_bool) {
                std::vector<bool> bools;
                for (const auto& elem : elements) {
                    bools.push_back(elem.asBool());
                }
                d = bools;
            } else {
                // Mixed types - store as object array
                d = elements;
            }

            return d;
        }

        Dictionary parse_inline_table() {
            if (get() != '{') {
                throw std::runtime_error(parse_error("expected '{'"));
            }

            Dictionary table;
            skip_ws_inline();

            while (peek() != '}') {
                std::string key = parse_key();
                skip_ws_inline();

                if (get() != '=') {
                    throw std::runtime_error(parse_error("expected '=' after key"));
                }

                skip_ws_inline();
                Dictionary value = parse_value();
                table[key] = value;

                skip_ws_inline();
                if (peek() == ',') {
                    get();
                    skip_ws_inline();
                } else if (peek() != '}') {
                    throw std::runtime_error(parse_error("expected ',' or '}' in inline table"));
                }
            }

            if (get() != '}') {
                throw std::runtime_error(parse_error("expected '}'"));
            }

            return table;
        }

        Dictionary parse_value() {
            skip_ws_inline();
            char c = peek();

            if (c == '"' || c == '\'') {
                Dictionary d;
                d = parse_string();
                return d;
            } else if (c == '[') {
                return parse_array();
            } else if (c == '{') {
                return parse_inline_table();
            } else if (c == 't' || c == 'f') {
                return parse_boolean();
            } else if (std::isdigit(static_cast<unsigned char>(c)) || c == '+' || c == '-' || c == 'i' || c == 'n') {
                // Could be number or datetime
                // Simple heuristic: if it contains ':', 'T', or 'Z', it's likely a datetime
                size_t lookahead = i;
                while (lookahead < s.size() && !std::isspace(static_cast<unsigned char>(s[lookahead])) && 
                       s[lookahead] != ',' && s[lookahead] != ']' && s[lookahead] != '}' && s[lookahead] != '#') {
                    if (s[lookahead] == 'T' || s[lookahead] == 'Z' || 
                        (s[lookahead] == ':' && lookahead > i + 2)) {
                        return parse_datetime();
                    }
                    ++lookahead;
                }
                return parse_number();
            } else {
                throw std::runtime_error(parse_error(std::string("unexpected character: '") + c + "'"));
            }
        }

        void set_value_at_path(Dictionary& root, const std::vector<std::string>& path, const Dictionary& value) {
            if (path.empty()) {
                throw std::runtime_error(parse_error("empty path"));
            }

            Dictionary* current = &root;
            for (size_t idx = 0; idx < path.size() - 1; ++idx) {
                const std::string& key = path[idx];
                if (!current->has(key)) {
                    (*current)[key] = Dictionary();
                }
                current = &(*current)[key];
            }

            (*current)[path.back()] = value;
        }

        Dictionary* get_current_table() {
            if (current_table.empty()) {
                return &root;
            }

            Dictionary* current = &root;
            for (const auto& key : current_table) {
                if (!current->has(key)) {
                    (*current)[key] = Dictionary();
                }
                current = &(*current)[key];
            }
            return current;
        }

        void parse_table_header() {
            if (get() != '[') {
                throw std::runtime_error(parse_error("expected '['"));
            }

            bool is_array_table = false;
            if (peek() == '[') {
                get();
                is_array_table = true;
            }

            std::vector<std::string> path;
            skip_ws_inline();

            while (peek() != ']') {
                path.push_back(parse_key());
                skip_ws_inline();

                if (peek() == '.') {
                    get();
                    skip_ws_inline();
                } else if (peek() != ']') {
                    throw std::runtime_error(parse_error("expected '.' or ']' in table header"));
                }
            }

            if (get() != ']') {
                throw std::runtime_error(parse_error("expected ']'"));
            }

            if (is_array_table) {
                if (get() != ']') {
                    throw std::runtime_error(parse_error("expected ']]' for array table"));
                }
            }

            current_table = path;

            // Create the table if it doesn't exist
            if (is_array_table) {
                // Array of tables - need to append a new table to an array
                Dictionary* current = &root;
                for (size_t idx = 0; idx < path.size() - 1; ++idx) {
                    if (!current->has(path[idx])) {
                        (*current)[path[idx]] = Dictionary();
                    }
                    current = &(*current)[path[idx]];
                }

                const std::string& last_key = path.back();
                if (!current->has(last_key)) {
                    (*current)[last_key] = std::vector<Dictionary>();
                }
                // For simplicity, we'll add a new empty table to the array
                // The actual values will be added by subsequent key-value pairs
                Dictionary new_table;
                auto& arr = (*current)[last_key];
                // Convert to object array if needed
                std::vector<Dictionary> tables;
                if (arr.isArrayObject()) {
                    tables = arr.asObjects();
                }
                tables.push_back(new_table);
                (*current)[last_key] = tables;
            } else {
                // Regular table - just ensure it exists
                get_current_table();
            }
        }

        void parse_key_value() {
            std::string key = parse_key();
            skip_ws_inline();

            if (get() != '=') {
                throw std::runtime_error(parse_error("expected '=' after key"));
            }

            skip_ws_inline();
            Dictionary value = parse_value();

            // Set the value in the current table
            Dictionary* table = get_current_table();
            (*table)[key] = value;
        }

        Dictionary parse() {
            root = Dictionary();
            skip_ws_and_comments();

            while (peek() != '\0') {
                if (peek() == '[') {
                    parse_table_header();
                } else if (is_bare_key_char(peek()) || peek() == '"' || peek() == '\'') {
                    parse_key_value();
                } else {
                    throw std::runtime_error(parse_error(std::string("unexpected character: '") + peek() + "'"));
                }

                skip_ws_and_comments();
            }

            return root;
        }
    };
}  // anonymous namespace

Dictionary parse_toml(const std::string& text) {
    TomlParser parser(text);
    return parser.parse();
}

}  // namespace ps
