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
            // build caret (simple: position columns as bytes)
            size_t caret_pos = err_col > 0 ? err_col - 1 : 0;
            if (caret_pos > line_text.size()) caret_pos = line_text.size();
            std::string caret(caret_pos, ' ');
            caret.push_back('^');

            std::ostringstream ss;
            ss << base << " (line " << err_line << ", column " << err_col << ")" << "\n";
            ss << line_text << "\n" << caret;
            if (!opener_stack.empty()) {
                auto o = opener_stack.back();
                ss << "\n(opened at line " << o.line << ", column " << o.col << ")";
            }
            return ss.str();
        }

        // compute line/col from absolute index
        std::pair<size_t,size_t> line_col_from_index(size_t idx) const {
            size_t pos = 0; size_t cur = 1; size_t col = 1;
            while (pos < idx && pos < s.size()) {
                if (s[pos] == '\n') { ++cur; col = 1; }
                else ++col;
                ++pos;
            }
            return {cur, col};
        }

        // find an unclosed '"' that starts before current parser index; return start index or npos
        size_t find_unclosed_string_before() const {
            if (i == 0) return std::string::npos;
            // scan backwards to find the nearest non-escaped '"'
            for (size_t k = i; k-- > 0; ) {
                if (s[k] != '"') continue;
                // is this quote escaped?
                size_t back = k;
                bool escaped = false;
                while (back > 0 && s[back-1] == '\\') { escaped = !escaped; --back; }
                if (escaped) continue;
                // found an opening quote at k; check if there's a closing quote before i
                bool closed = false;
                for (size_t j = k + 1; j < i && j < s.size(); ++j) {
                    if (s[j] == '"') {
                        // check if this quote is escaped
                        size_t back2 = j;
                        bool esc2 = false;
                        while (back2 > 0 && s[back2-1] == '\\') { esc2 = !esc2; --back2; }
                        if (!esc2) { closed = true; break; }
                    }
                }
                if (!closed) return k;
                // otherwise continue searching further back
            }
            return std::string::npos;
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
            // Provide friendly suggestions for common mistakes: Python-style True/False or unquoted paths/identifiers
            if (std::isalpha(static_cast<unsigned char>(c))) {
                // capture a short token to inspect
                size_t j = i;
                while (j < s.size() && (std::isalnum(static_cast<unsigned char>(s[j])) || s[j] == '_' || s[j] == '/' || s[j] == '.' || s[j] == '-' )) ++j;
                std::string token = s.substr(i, j - i);
                if (token == "True" || token == "False") {
                    std::string sug = (token == "True") ? "true" : "false";
                    throw JsonParseError(format_error(std::string("unexpected token while parsing value — did you mean '") + sug + "' (lowercase)?", line, col), line, col);
                }
                // path-like or dotted identifiers that should probably be quoted
                if (token.find('/') != std::string::npos || token.find('.') != std::string::npos) {
                    throw JsonParseError(format_error(std::string("unexpected token while parsing value — unquoted path/identifier '") + token + "'; did you mean to quote it?", line, col), line, col);
                }
            }
            throw JsonParseError(format_error("unexpected token while parsing value", line, col), line, col);
        }

        Value parse_null() {
            if (s.compare(i, 4, "null") == 0) { i += 4; col += 4; return Value(); }
            throw JsonParseError(format_error("invalid literal", line, col), line, col);
        }

        Value parse_bool() {
            if (s.compare(i, 4, "true") == 0) { i += 4; col += 4; return Value(true); }
            if (s.compare(i, 5, "false") == 0) { i += 5; col += 5; return Value(false); }
            throw JsonParseError(format_error("invalid literal", line, col), line, col);
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
                throw JsonParseError(format_error("invalid number", line, col), line, col);
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
                if (!std::isdigit(static_cast<unsigned char>(peek()))) throw JsonParseError(format_error("invalid number", line, col), line, col);
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
                std::string base;
                if (c == ':') base = "unexpected ':' after value; found key/value pair inside array";
                else base = "expected ',' or ']'";
                {
                    std::string msg = format_error(base, line, col);
                    throw JsonParseError(msg, line, col);
                }
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
                if (peek() != '"') {
                    // attempt to read an identifier to provide a helpful suggestion
                    size_t start = i;
                    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
                    size_t j = start;
                    while (j < s.size() && (std::isalnum(static_cast<unsigned char>(s[j])) || s[j] == '_')) ++j;
                    std::string ident;
                    if (j > start) ident = s.substr(start, j - start);
                    std::string base = "expected string key";
                    if (!ident.empty()) base += std::string(" — are you missing quotes around '") + ident + "'?";
                    throw JsonParseError(format_error(base, line, col), line, col);
                }
                Value k = parse_string();
                skip_ws();
                if (get() != ':') throw JsonParseError(format_error("expected ':' after object key", line, col), line, col);
                skip_ws();
                Value v = parse_value();
                d[k.asString()] = v;
                skip_ws();
                char c = peek();
                if (c == '}') { get(); pop_opener('}'); break; }
                if (c == ',') { get(); skip_ws(); continue; }
                // Allow implicit separator between object members: next token is a string key
                if (c == '"') {
                    // treat as implicit separator (missing comma)
                    continue;
                }
                {
                    std::string base = "expected ',' or '}'";
                    // if there's an unclosed string earlier, suggest a missing quote
                    size_t us = find_unclosed_string_before();
                    if (us != std::string::npos) {
                        // extract the partial string content between the opening quote and current parse index
                        size_t content_start = us + 1;
                        // find a closing quote before current index if any
                        size_t closeq = std::string::npos;
                        for (size_t k = content_start; k < i && k < s.size(); ++k) {
                            if (s[k] == '"') {
                                // ensure it's not escaped
                                size_t back = k; bool esc = false; while (back > content_start && s[back-1] == '\\') { esc = !esc; --back; }
                                if (!esc) { closeq = k; break; }
                            }
                        }
                        size_t content_end = (closeq != std::string::npos) ? closeq : (i < s.size() ? i : s.size());
                        if (content_end < content_start) content_end = content_start;
                        std::string snippet = s.substr(content_start, content_end - content_start);
                        // trim whitespace and trailing comma
                        auto trim = [&](std::string &t){ size_t a=0; while (a<t.size() && std::isspace(static_cast<unsigned char>(t[a]))) ++a; size_t b=t.size(); while (b>a && std::isspace(static_cast<unsigned char>(t[b-1]))) --b; t = t.substr(a,b-a); if (!t.empty() && t.back()==',') t.pop_back(); };
                        trim(snippet);
                        if (snippet.size() > 80) snippet = snippet.substr(0, 77) + "...";
                        if (snippet.empty() || (snippet.find(":")!=std::string::npos)) {
                            // fallback: extract the whole line where the unclosed quote started
                            size_t line_s = us;
                            while (line_s > 0 && s[line_s-1] != '\n') --line_s;
                            size_t line_e = us;
                            while (line_e < s.size() && s[line_e] != '\n') ++line_e;
                            std::string line_text = s.substr(line_s, line_e - line_s);
                            // remove leading up to the opening quote
                            size_t qpos = line_text.find('"');
                            if (qpos != std::string::npos) {
                                std::string after = line_text.substr(qpos + 1);
                                // trim
                                size_t aa = 0; while (aa < after.size() && std::isspace(static_cast<unsigned char>(after[aa]))) ++aa;
                                size_t bb = after.size(); while (bb > aa && std::isspace(static_cast<unsigned char>(after[bb-1]))) --bb;
                                snippet = after.substr(aa, bb - aa);
                                if (snippet.size() > 80) snippet = snippet.substr(0, 77) + "...";
                            }
                            // If that still looks structural (e.g. 'notes": ['), try to find the nearest "description" key earlier
                            if (snippet.empty() || snippet.find(":")!=std::string::npos) {
                                size_t desc_pos = s.rfind("\"description\"", us);
                                if (desc_pos != std::string::npos) {
                                    // find colon after desc_pos
                                    size_t colon = s.find(':', desc_pos + 13);
                                    if (colon != std::string::npos) {
                                        // find opening quote after colon
                                        size_t q = s.find('"', colon+1);
                                        if (q != std::string::npos && q < i) {
                                            size_t qend = i;
                                            if (qend > q+1) {
                                                std::string val = s.substr(q+1, qend - (q+1));
                                                // trim
                                                size_t ta=0; while (ta<val.size() && std::isspace(static_cast<unsigned char>(val[ta]))) ++ta;
                                                size_t tb=val.size(); while (tb>ta && std::isspace(static_cast<unsigned char>(val[tb-1]))) --tb;
                                                snippet = val.substr(ta, tb-ta);
                                                if (snippet.size() > 80) snippet = snippet.substr(0,77)+"...";
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        base += std::string(" — is there a missing closing quote on '") + snippet + "'?";
                    }
                    std::string msg = format_error(base, line, col);
                    throw JsonParseError(msg, line, col);
                }
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
                if (p.peek() != '"') throw JsonParseError(p.format_error("expected string key in top-level implicit object", p.line, p.col), p.line, p.col);
                Value k = p.parse_string();
                p.skip_ws();
                if (p.get() != ':') throw JsonParseError(p.format_error("expected ':' after object key", p.line, p.col), p.line, p.col);
                p.skip_ws();
                Value v = p.parse_value();
                root[k.asString()] = v;
                p.skip_ws();
                char c = p.peek();
                if (c == ',') { p.get(); p.skip_ws(); continue; }
                if (c == '\0') break;
                // allow implicit separator, otherwise error
                if (c == '"') continue;
                throw JsonParseError(p.format_error("extra data after JSON value", p.line, p.col), p.line, p.col);
            }
            return Value(std::move(root));
        }
        throw JsonParseError(p.format_error("extra data after JSON value", p.line, p.col), p.line, p.col);
    }
    return val;
}

} // namespace ps
