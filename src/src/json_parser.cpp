// moved json parser implementation
#include <ps/simple_json.hpp>
#include <cctype>
#include <stdexcept>
#include <sstream>

namespace ps {

namespace {
    struct Parser {
        const std::string& s;
        size_t i = 0;

        Parser(const std::string& str) : s(str) {}

        void skip_ws() {
            while (i < s.size()) {
                unsigned char c = static_cast<unsigned char>(s[i]);
                if (std::isspace(c)) { ++i; continue; }
                // line comment //...
                if (c == '/' && i + 1 < s.size() && s[i+1] == '/') {
                    i += 2;
                    while (i < s.size() && s[i] != '\n') ++i;
                    continue;
                }
                // block comment /* ... */
                if (c == '/' && i + 1 < s.size() && s[i+1] == '*') {
                    i += 2;
                    bool closed = false;
                    while (i + 1 < s.size()) {
                        if (s[i] == '*' && s[i+1] == '/') { i += 2; closed = true; break; }
                        ++i;
                    }
                    if (!closed) throw std::runtime_error("unterminated comment");
                    continue;
                }
                break;
            }
        }

        char peek() const { return i < s.size() ? s[i] : '\0'; }
        char get() { return i < s.size() ? s[i++] : '\0'; }

        Value parse_value() {
            skip_ws();
            char c = peek();
            if (c == 'n') return parse_null();
            if (c == 't' || c == 'f') return parse_bool();
            if (c == '"') return parse_string();
            if (c == '[') return parse_array();
            if (c == '{') return parse_object();
            if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number();
            throw std::runtime_error("unexpected token while parsing value");
        }

        Value parse_null() {
            if (s.compare(i, 4, "null") == 0) { i += 4; return Value(); }
            throw std::runtime_error("invalid literal");
        }

        Value parse_bool() {
            if (s.compare(i, 4, "true") == 0) { i += 4; return Value(true); }
            if (s.compare(i, 5, "false") == 0) { i += 5; return Value(false); }
            throw std::runtime_error("invalid literal");
        }

        Value parse_string() {
            if (get() != '"') throw std::runtime_error("expected '\"'");
            std::string out;
            while (true) {
                char c = get();
                if (c == '\0') throw std::runtime_error("unexpected end in string");
                if (c == '"') break;
                if (c == '\\') {
                    char e = get();
                    switch (e) {
                        case '"': out.push_back('"'); break;
                        case '\\': out.push_back('\\'); break;
                        case '/': out.push_back('/'); break;
                        case 'b': out.push_back('\b'); break;
                        case 'f': out.push_back('\f'); break;
                        case 'n': out.push_back('\n'); break;
                        case 'r': out.push_back('\r'); break;
                        case 't': out.push_back('\t'); break;
                        default: throw std::runtime_error("unsupported escape");
                    }
                } else {
                    out.push_back(c);
                }
            }
            return Value(std::move(out));
        }

        Value parse_number() {
            size_t start = i;
            if (peek() == '-') ++i;
            while (std::isdigit(static_cast<unsigned char>(peek()))) ++i;
            bool is_float = false;
            if (peek() == '.') {
                is_float = true; ++i;
                while (std::isdigit(static_cast<unsigned char>(peek()))) ++i;
            }
            if (peek() == 'e' || peek() == 'E') {
                is_float = true; ++i;
                if (peek() == '+' || peek() == '-') ++i;
                while (std::isdigit(static_cast<unsigned char>(peek()))) ++i;
            }
            std::string token = s.substr(start, i - start);
            if (is_float) {
                double d; std::istringstream ss(token); ss >> d; return Value(d);
            } else {
                int64_t v; std::istringstream ss(token); ss >> v; return Value(v);
            }
        }

        Value parse_array() {
            if (get() != '[') throw std::runtime_error("expected '['");
            Value::list_t out;
            skip_ws();
            if (peek() == ']') { get(); return Value(std::move(out)); }
            while (true) {
                out.emplace_back(parse_value());
                skip_ws();
                char c = get();
                if (c == ']') break;
                if (c != ',') throw std::runtime_error("expected ',' or ']'");
            }
            return Value(std::move(out));
        }

        Value parse_object() {
            if (get() != '{') throw std::runtime_error("expected '{'");
            Dictionary d;
            skip_ws();
            if (peek() == '}') { get(); return Value(std::move(d)); }
            while (true) {
                skip_ws();
                if (peek() != '"') throw std::runtime_error("expected string key");
                Value k = parse_string();
                skip_ws();
                if (get() != ':') throw std::runtime_error("expected ':'");
                Value v = parse_value();
                d[k.as_string()] = v;
                skip_ws();
                char c = get();
                if (c == '}') break;
                if (c != ',') throw std::runtime_error("expected ',' or '}'");
            }
            return Value(std::move(d));
        }
    };
}

Value parse_json(const std::string& text) {
    Parser p(text);
    auto val = p.parse_value();
    p.skip_ws();
    if (p.peek() != '\0') throw std::runtime_error("extra data after JSON value");
    return val;
}

} // namespace ps
