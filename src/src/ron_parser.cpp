#include <ps/ron.h>
#include <cctype>
#include <sstream>

namespace ps {

namespace {
    struct RonParser {
        const std::string& s;
        size_t i = 0;

        RonParser(const std::string& str) : s(str) {}

        char peek() const { return i < s.size() ? s[i] : '\0'; }
        char get() { return i < s.size() ? s[i++] : '\0'; }

        void skip_ws() {
            while (i < s.size()) {
                unsigned char c = static_cast<unsigned char>(s[i]);
                if (std::isspace(c)) { ++i; continue; }
                if (c == '/' and i + 1 < s.size() and s[i+1] == '/') {
                    i += 2; while (i < s.size() and s[i] != '\n') ++i; continue;
                }
                if (c == '/' and i + 1 < s.size() and s[i+1] == '*') {
                    i += 2; while (i + 1 < s.size() and !(s[i] == '*' and s[i+1] == '/')) ++i; if (i + 1 < s.size()) i += 2; continue;
                }
                break;
            }
        }


        Value parse_string() {
            if (get() != '"') throw std::runtime_error("expected string");
            std::string out;
            while (true) {
                char c = get(); if (c == '\0') throw std::runtime_error("unterminated string");
                if (c == '"') break;
                if (c == '\\') {
                    char e = get(); if (e == 'n') out.push_back('\n'); else out.push_back(e);
                } else out.push_back(c);
            }
            return Value(std::move(out));
        }

        Value parse_number_or_ident() {
            size_t start = i;
            while (std::isalnum(static_cast<unsigned char>(peek())) or peek() == '.' or peek() == '_' or peek() == '-') get();
            std::string tok = s.substr(start, i - start);
            if (tok == "null") return Value();
            if (tok == "true") return Value(true);
            if (tok == "false") return Value(false);
            // try integer
            std::istringstream ss(tok);
            if (tok.find('.') != std::string::npos) { double d; ss >> d; return Value(d); }
            int64_t v; ss >> v; if (!ss.fail()) return Value(v);
            return Value(tok);
        }

        Value parse_array() {
            if (get() != '[') throw std::runtime_error("expected '['");
            std::vector<Value> out_values;
            std::vector<Dictionary> out_dicts;
            bool all_objects = true;
            skip_ws();
            if (peek() == ']') { get(); return Value(std::move(out_values)); }
            while (true) {
                Value v = parse_value();
                // accumulate
                out_values.emplace_back(v);
                if (v.isDict()) {
                    // safe to dereference shared_ptr
                    out_dicts.push_back(*v.asDict());
                } else {
                    all_objects = false;
                }
                skip_ws();
                if (peek() == ',') { get(); skip_ws(); if (peek() == ']') { get(); break; } continue; }
                if (peek() == ']') { get(); break; }
                // allow implicit separator
                continue;
            }
            if (all_objects) {
                return Value(std::move(out_dicts));
            }
            return Value(std::move(out_values));
        }

        std::string parse_key() {
            skip_ws();
            if (peek() == '"') { auto v = parse_string(); return v.asString(); }
            // identifier key
            size_t start = i;
            while (std::isalnum(static_cast<unsigned char>(peek())) or peek() == '_' ) get();
            return s.substr(start, i - start);
        }

        Value parse_object() {
            if (get() != '{') throw std::runtime_error("expected '{'");
            Dictionary d;
            skip_ws();
            if (peek() == '}') { get(); return Value(std::move(d)); }
            while (true) {
                std::string key = parse_key();
                skip_ws();
                    if (peek() == ':' or peek() == '=') get(); else {
                        size_t ctx_s = (i >= 20) ? i - 20 : 0;
                        size_t ctx_e = i + 20;
                        if (ctx_e > s.size()) ctx_e = s.size();
                        std::string snippet = s.substr(ctx_s, ctx_e - ctx_s);
                        std::ostringstream msg;
                        msg << "expected ':' or '=' after key near '" << snippet << "'";
                        throw std::runtime_error(msg.str());
                    }
                skip_ws();
                Value v = parse_value();
                d[key] = v;
                skip_ws();
                if (peek() == ',') { get(); skip_ws(); if (peek() == '}') { get(); break; } continue; }
                if (peek() == '}') { get(); break; }
                // allow implicit separator
                continue;
            }
            return Value(std::move(d));
        }

        Value parse_value() {
            skip_ws();
            char c = peek();
            if (c == '{') return parse_object();
            if (c == '[') return parse_array();
            if (c == '"') return parse_string();
            if (std::isalpha(static_cast<unsigned char>(c)) or c == '_' or c == '-' or std::isdigit(static_cast<unsigned char>(c))) return parse_number_or_ident();
            {
                size_t ctx_s = (i >= 20) ? i - 20 : 0;
                size_t ctx_e = i + 20;
                if (ctx_e > s.size()) ctx_e = s.size();
                std::string snippet = s.substr(ctx_s, ctx_e - ctx_s);
                std::ostringstream msg;
                char pc = peek();
                if (pc == '\0') msg << "unexpected end of input in RON";
                else msg << "unexpected token in RON at index " << i << " ('" << pc << "') near '" << snippet << "'";
                throw std::runtime_error(msg.str());
            }
        }
    };
}

Dictionary parse_ron(const std::string& text) {
    RonParser p(text);
    p.skip_ws();
    // If the first non-ws char is an identifier or quoted string, treat as implicit root object
    char c = p.peek();
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '"') {
        Dictionary root;
        while (p.peek() != '\0') {
            std::string key = p.parse_key();
            p.skip_ws();
            if (p.peek() == ':' || p.peek() == '=') p.get();
            else throw std::runtime_error("expected ':' or '=' after key");
            p.skip_ws();
            Value v = p.parse_value();
            root[key] = v;
            p.skip_ws();
            if (p.peek() == ',') { p.get(); p.skip_ws(); continue; }
            // allow implicit separator
            if (std::isalpha(static_cast<unsigned char>(p.peek())) || p.peek() == '_' || p.peek() == '"') continue;
            break;
        }
        return *Value(std::move(root)).asDict();
    }
    return *p.parse_value().asDict();
}

} // namespace ps
