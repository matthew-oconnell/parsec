// moved json parser implementation
#include <ps/simple_json.hpp>
#include <cctype>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <iomanip>

namespace ps {

namespace {
    struct JsonParseError : public std::runtime_error {
        size_t line, col;
        JsonParseError(const std::string& msg, size_t l, size_t c)
            : std::runtime_error(msg), line(l), col(c) {}
    };

    struct Parser {
        const std::string& s;
        size_t i = 0;
        size_t line = 1;
        size_t col = 1;

        struct Opener { char ch; size_t line, col; };
        std::vector<Opener> opener_stack;

        Parser(const std::string& str) : s(str) {}

        char peek() const { return i < s.size() ? s[i] : '\0'; }

        char get() {
            if (i >= s.size()) return '\0';
            char c = s[i++];
            if (c == '\n') { ++line; col = 1; }
            else ++col;
            return c;
        }

        void push_opener(char ch) { opener_stack.push_back(Opener{ch, line, col}); }
        void pop_opener(char expected) {
            if (opener_stack.empty()) return;
            opener_stack.pop_back();
        }

        void skip_ws() {
            while (i < s.size()) {
                unsigned char c = static_cast<unsigned char>(s[i]);
                if (std::isspace(c)) { get(); continue; }

                // line comment //...
                if (c == '/' && i + 1 < s.size() && s[i+1] == '/') {
                    get(); get(); // consume //
                    while (i < s.size() && peek() != '\n') get();
                    continue;
                }

                // block comment /* ... */
                if (c == '/' && i + 1 < s.size() && s[i+1] == '*') {
                    get(); get(); // consume /*
                    bool closed = false;
                    while (i < s.size()) {
                        char a = get();
                        if (a == '*' && peek() == '/') { get(); closed = true; break; }
                    }
                    if (!closed) throw JsonParseError("unterminated block comment", line, col);
                    continue;
                }

                break;
            }
        }

        Value parse_value() {
            skip_ws();
            char c = peek();
            if (c == 'n') return parse_null();
            if (c == 't' || c == 'f') return parse_bool();
            if (c == '"') return parse_string();
            if (c == '[') return parse_array();
            if (c == '{') return parse_object();
            if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number();
            throw JsonParseError("unexpected token while parsing value", line, col);
        }

        Value parse_null() {
            if (s.compare(i, 4, "null") == 0) { i += 4; col += 4; return Value(); }
            throw JsonParseError("invalid literal", line, col);
        }

        Value parse_bool() {
            if (s.compare(i, 4, "true") == 0) { i += 4; col += 4; return Value(true); }
            if (s.compare(i, 5, "false") == 0) { i += 5; col += 5; return Value(false); }
            throw JsonParseError("invalid literal", line, col);
        }

        // helper: parse hex digit
        static int hex_val(char c) {
            if ('0' <= c && c <= '9') return c - '0';
            if ('a' <= c && c <= 'f') return 10 + (c - 'a');
            if ('A' <= c && c <= 'F') return 10 + (c - 'A');
            return -1;
        }

        // encode a Unicode code point as UTF-8 into out
        static void encode_utf8(uint32_t cp, std::string& out) {
            if (cp <= 0x7F) out.push_back(static_cast<char>(cp));
            else if (cp <= 0x7FF) {
                out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            } else if (cp <= 0xFFFF) {
                out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            } else {
                out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
        }

        Value parse_string() {
            if (get() != '"') throw JsonParseError("expected '\"'", line, col);
            std::string out;
            while (true) {
                char c = get();
                if (c == '\0') throw JsonParseError("unexpected end in string", line, col);
                if (c == '"') break;
                if (c == '\\') {
                    char e = get();
                    if (e == '\0') throw JsonParseError("unexpected end in string escape", line, col);
                    switch (e) {
                        case '"': out.push_back('"'); break;
                        case '\\': out.push_back('\\'); break;
                        case '/': out.push_back('/'); break;
                        case 'b': out.push_back('\b'); break;
                        case 'f': out.push_back('\f'); break;
                        case 'n': out.push_back('\n'); break;
                        case 'r': out.push_back('\r'); break;
                        case 't': out.push_back('\t'); break;
                        case 'u': {
                            // parse 4 hex digits
                            int v = 0;
                            for (int k = 0; k < 4; ++k) {
                                char h = get();
                                if (h == '\0') throw JsonParseError("unterminated unicode escape", line, col);
                                int hv = hex_val(h);
                                if (hv < 0) throw JsonParseError("invalid unicode escape", line, col);
                                v = (v << 4) | hv;
                            }
                            encode_utf8(static_cast<uint32_t>(v), out);
                            break;
                        }
                        default:
                            throw JsonParseError("unsupported escape sequence", line, col);
                    }
                } else {
                    out.push_back(c);
                }
            }
            return Value(std::move(out));
        }

        Value parse_number() {
            size_t start = i;
            if (peek() == '-') { get(); }
            if (!std::isdigit(static_cast<unsigned char>(peek())))
                throw JsonParseError("invalid number", line, col);
            while (std::isdigit(static_cast<unsigned char>(peek()))) get();
            bool is_float = false;
            if (peek() == '.') {
                is_float = true; get();
                if (!std::isdigit(static_cast<unsigned char>(peek()))) throw JsonParseError("invalid number", line, col);
                while (std::isdigit(static_cast<unsigned char>(peek()))) get();
            }
            if (peek() == 'e' || peek() == 'E') {
                is_float = true; get();
                if (peek() == '+' || peek() == '-') get();
                if (!std::isdigit(static_cast<unsigned char>(peek()))) throw JsonParseError("invalid number", line, col);
                while (std::isdigit(static_cast<unsigned char>(peek()))) get();
            }
            std::string token = s.substr(start, i - start);
            if (is_float) {
                double d; std::istringstream ss(token); ss >> d; return Value(d);
            } else {
                int64_t v; std::istringstream ss(token); ss >> v; return Value(v);
            }
        }

        Value parse_array() {
            if (get() != '[') throw JsonParseError("expected '['", line, col);
            // push opener for diagnostics
            opener_stack.push_back(Opener{'[', line, col});
            Value::list_t out;
            skip_ws();
            if (peek() == ']') { get(); pop_opener(']'); return Value(std::move(out)); }
            while (true) {
                out.emplace_back(parse_value());
                skip_ws();
                char c = peek();
                if (c == ']') { get(); pop_opener(']'); break; }
                if (c == ',') { get(); skip_ws(); continue; }
                // Allow implicit separator: if the next token looks like the start of a value, accept it
                if (c == '{' || c == '[' || c == '"' || c == 'n' || c == 't' || c == 'f' || c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
                    // treat as implicit separator (missing comma)
                    continue;
                }
                std::ostringstream ss; ss << "expected ',' or ']' at " << line << ":" << col;
                if (!opener_stack.empty()) {
                    auto o = opener_stack.back(); ss << " (opened at " << o.line << ":" << o.col << ")";
                }
                throw JsonParseError(ss.str(), line, col);
            }
            return Value(std::move(out));
        }

        Value parse_object() {
            if (get() != '{') throw JsonParseError("expected '{'", line, col);
            opener_stack.push_back(Opener{'{', line, col});
            Dictionary d;
            skip_ws();
            if (peek() == '}') { get(); pop_opener('}'); return Value(std::move(d)); }
            while (true) {
                skip_ws();
                if (peek() != '"') throw JsonParseError("expected string key", line, col);
                Value k = parse_string();
                skip_ws();
                if (get() != ':') throw JsonParseError("expected ':' after object key", line, col);
                skip_ws();
                Value v = parse_value();
                d[k.as_string()] = v;
                skip_ws();
                char c = peek();
                if (c == '}') { get(); pop_opener('}'); break; }
                if (c == ',') { get(); skip_ws(); continue; }
                // Allow implicit separator between object members: next token is a string key
                if (c == '"') {
                    // treat as implicit separator (missing comma)
                    continue;
                }
                std::ostringstream ss; ss << "expected ',' or '}' at " << line << ":" << col;
                if (!opener_stack.empty()) {
                    auto o = opener_stack.back(); ss << " (opened at " << o.line << ":" << o.col << ")";
                }
                throw JsonParseError(ss.str(), line, col);
            }
            return Value(std::move(d));
        }
    };
}

Value parse_json(const std::string& text) {
    Parser p(text);
    auto val = p.parse_value();
    p.skip_ws();
    if (p.peek() != '\0') {
        // Tolerate a top-level sequence of object members without surrounding braces
        // e.g. "key": value  "key2": value2
        // If the input starts with a string followed by ':', treat as an implicit root object
        size_t save_i = p.i;
        char first_nonws = '\0';
        for (size_t k = 0; k < text.size(); ++k) {
            if (!std::isspace(static_cast<unsigned char>(text[k]))) { first_nonws = text[k]; break; }
        }
        if (first_nonws == '"') {
            // parse remaining as object members
            // build a Dictionary with the already-parsed value if it was a dict, else include it if appropriate
            p.i = 0;
            p.line = 1; p.col = 1;
            Dictionary root;
            p.skip_ws();
            while (p.peek() != '\0') {
                p.skip_ws();
                if (p.peek() != '"') throw JsonParseError("expected string key in top-level implicit object", p.line, p.col);
                Value k = p.parse_string();
                p.skip_ws();
                if (p.get() != ':') throw JsonParseError("expected ':' after object key", p.line, p.col);
                p.skip_ws();
                Value v = p.parse_value();
                root[k.as_string()] = v;
                p.skip_ws();
                char c = p.peek();
                if (c == ',') { p.get(); p.skip_ws(); continue; }
                if (c == '\0') break;
                // allow implicit separator, otherwise error
                if (c == '"') continue;
                throw JsonParseError("extra data after JSON value", p.line, p.col);
            }
            return Value(std::move(root));
        }
        throw JsonParseError("extra data after JSON value", p.line, p.col);
    }
    return val;
}

} // namespace ps
