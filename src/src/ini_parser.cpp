#include <ps/ini.h>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace ps {

namespace {
    struct IniParser {
        const std::string& s;
        size_t i = 0;
        size_t line = 1;
        size_t column = 1;
        std::string current_section;

        IniParser(const std::string& str) : s(str) {}

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

        void skip_to_eol() {
            while (peek() != '\0' && peek() != '\n') {
                get();
            }
        }

        std::string parse_error(const std::string& msg) {
            std::ostringstream ss;
            ss << "INI parse error: " << msg << " (line " << line << ", column " << column << ")";
            return ss.str();
        }

        std::string trim(const std::string& str) {
            size_t start = 0;
            while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
                ++start;
            }
            size_t end = str.size();
            while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
                --end;
            }
            return str.substr(start, end - start);
        }

        std::string parse_section_name() {
            if (get() != '[') {
                throw std::runtime_error(parse_error("expected '['"));
            }

            std::string name;
            while (peek() != ']' && peek() != '\0' && peek() != '\n') {
                name += get();
            }

            if (peek() != ']') {
                throw std::runtime_error(parse_error("unterminated section name, expected ']'"));
            }
            get(); // consume ']'

            return trim(name);
        }

        std::pair<std::string, std::string> parse_key_value() {
            // Parse key
            std::string key;
            while (peek() != '=' && peek() != ':' && peek() != '\0' && peek() != '\n') {
                key += get();
            }

            key = trim(key);
            if (key.empty()) {
                throw std::runtime_error(parse_error("empty key"));
            }

            char separator = peek();
            if (separator != '=' && separator != ':') {
                throw std::runtime_error(parse_error("expected '=' or ':' after key"));
            }
            get(); // consume separator

            skip_ws_inline();

            // Parse value (rest of line, trimmed)
            std::string value;
            while (peek() != '\0' && peek() != '\n' && peek() != ';' && peek() != '#') {
                value += get();
            }

            return {key, trim(value)};
        }

        Dictionary parse_value(const std::string& str) {
            // Try to detect type and convert
            if (str.empty()) {
                Dictionary d;
                d = std::string("");
                return d;
            }

            // Check for boolean
            std::string lower = str;
            for (char& c : lower) {
                c = std::tolower(static_cast<unsigned char>(c));
            }
            
            if (lower == "true" || lower == "yes" || lower == "on" || lower == "1") {
                Dictionary d;
                d = true;
                return d;
            }
            if (lower == "false" || lower == "no" || lower == "off" || lower == "0") {
                Dictionary d;
                d = false;
                return d;
            }

            // Try integer or float, but avoid version strings like "1.0.0"
            bool is_int = true;
            bool is_float = false;
            size_t idx = 0;
            int dot_count = 0;
            
            if (str[idx] == '+' || str[idx] == '-') ++idx;
            
            if (idx >= str.size() || !std::isdigit(static_cast<unsigned char>(str[idx]))) {
                is_int = false;
            } else {
                while (idx < str.size()) {
                    char c = str[idx];
                    if (std::isdigit(static_cast<unsigned char>(c))) {
                        ++idx;
                    } else if (c == '.') {
                        ++dot_count;
                        if (dot_count > 1) {
                            // Multiple dots means version string, not a number
                            is_int = false;
                            is_float = false;
                            break;
                        }
                        is_float = true;
                        is_int = false;
                        ++idx;
                    } else if (c == 'e' || c == 'E') {
                        is_float = true;
                        is_int = false;
                        break;
                    } else {
                        is_int = false;
                        break;
                    }
                }
            }

            if (is_int && idx == str.size()) {
                try {
                    int64_t val = std::stoll(str);
                    Dictionary d;
                    d = val;
                    return d;
                } catch (...) {
                    // Fall through to string
                }
            }

            // Try float - only if we haven't detected multiple dots (version string)
            if (is_float && dot_count == 1) {
                try {
                    double val = std::stod(str);
                    Dictionary d;
                    d = val;
                    return d;
                } catch (...) {
                    // Fall through to string
                }
            }

            // Default to string
            Dictionary d;
            
            // Remove quotes if present
            std::string unquoted = str;
            if (unquoted.size() >= 2) {
                if ((unquoted.front() == '"' && unquoted.back() == '"') ||
                    (unquoted.front() == '\'' && unquoted.back() == '\'')) {
                    unquoted = unquoted.substr(1, unquoted.size() - 2);
                }
            }
            
            d = unquoted;
            return d;
        }

        Dictionary parse() {
            Dictionary root;
            Dictionary* current_dict = &root;

            while (peek() != '\0') {
                skip_ws_inline();

                char c = peek();
                
                // Skip empty lines
                if (c == '\n' || c == '\r') {
                    get();
                    continue;
                }

                // Skip comments
                if (c == ';' || c == '#') {
                    skip_to_eol();
                    continue;
                }

                // Section header
                if (c == '[') {
                    current_section = parse_section_name();
                    
                    // Create nested structure for dotted sections
                    if (current_section.find('.') != std::string::npos) {
                        // Support dotted section names like [section.subsection]
                        std::vector<std::string> parts;
                        std::string part;
                        for (char ch : current_section) {
                            if (ch == '.') {
                                if (!part.empty()) {
                                    parts.push_back(trim(part));
                                    part.clear();
                                }
                            } else {
                                part += ch;
                            }
                        }
                        if (!part.empty()) {
                            parts.push_back(trim(part));
                        }

                        current_dict = &root;
                        for (const auto& p : parts) {
                            if (!current_dict->has(p)) {
                                (*current_dict)[p] = Dictionary();
                            }
                            current_dict = &(*current_dict)[p];
                        }
                    } else {
                        if (!root.has(current_section)) {
                            root[current_section] = Dictionary();
                        }
                        current_dict = &root[current_section];
                    }

                    skip_ws_inline();
                    if (peek() == '\n' || peek() == '\r') {
                        get();
                    }
                    continue;
                }

                // Key-value pair
                auto [key, value_str] = parse_key_value();
                (*current_dict)[key] = parse_value(value_str);

                // Skip to end of line (handles inline comments)
                skip_ws_inline();
                if (peek() == ';' || peek() == '#') {
                    skip_to_eol();
                }
                if (peek() == '\n' || peek() == '\r') {
                    get();
                }
            }

            return root;
        }
    };
}  // anonymous namespace

Dictionary parse_ini(const std::string& text) {
    IniParser parser(text);
    return parser.parse();
}

}  // namespace ps
