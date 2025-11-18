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
                if (std::isspace(c)) {
                    ++i;
                    continue;
                }
                if (c == '/' and i + 1 < s.size() and s[i + 1] == '/') {
                    i += 2;
                    while (i < s.size() and s[i] != '\n') ++i;
                    continue;
                }
                if (c == '/' and i + 1 < s.size() and s[i + 1] == '*') {
                    i += 2;
                    while (i + 1 < s.size() and !(s[i] == '*' and s[i + 1] == '/')) ++i;
                    if (i + 1 < s.size()) i += 2;
                    continue;
                }
                break;
            }
        }

        Dictionary parse_string() {
            if (get() != '"') throw std::runtime_error("expected string");
            std::string out;
            while (true) {
                char c = get();
                if (c == '\0') throw std::runtime_error("unterminated string");
                if (c == '"') break;
                if (c == '\\') {
                    char e = get();
                    if (e == 'n')
                        out.push_back('\n');
                    else
                        out.push_back(e);
                } else
                    out.push_back(c);
            }
            Dictionary d;
            d = std::move(out);
            return d;
        }

        Dictionary parse_number_or_ident() {
            size_t start = i;
            while (std::isalnum(static_cast<unsigned char>(peek())) or peek() == '.' or peek() == '_' or peek() == '-')
                get();
            std::string tok = s.substr(start, i - start);
            if (tok == "null") return Dictionary::null();
            if (tok == "true") {
                Dictionary d;
                d = true;
                return d;
            }
            if (tok == "false") {
                Dictionary d;
                d = false;
                return d;
            }
            // try integer or double
            std::istringstream ss(tok);
            if (tok.find('.') != std::string::npos) {
                double dval;
                ss >> dval;
                Dictionary d;
                d = dval;
                return d;
            }
            int64_t v;
            ss >> v;
            if (!ss.fail()) {
                Dictionary d;
                d = v;
                return d;
            }
            Dictionary d;
            d = tok;
            return d;
        }

        Dictionary parse_array() {
            if (get() != '[') throw std::runtime_error("expected '['");
            std::vector<Dictionary> out_values;
            bool allInt = true, allDouble = true, allString = true, allBool = true, allObject = true;
            skip_ws();
            if (peek() == ']') {
                get();
                Dictionary res;
                res = std::vector<Dictionary>{};
                return res;
            }
            while (true) {
                Dictionary v = parse_value();
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
                        allInt = allDouble = allString = allBool = false;
                        allObject = false;
                        break;
                }
                skip_ws();
                if (peek() == ']') {
                    get();
                    break;
                }
                if (peek() == ',') {
                    get();
                    skip_ws();
                    continue;
                }
                // allow implicit separator
                continue;
            }
            Dictionary res;
            if (out_values.empty()) {
                res = std::vector<Dictionary>{};
                return res;
            }
            if (allInt) {
                std::vector<int> iv;
                iv.reserve(out_values.size());
                for (auto const& e : out_values) iv.push_back(e.asInt());
                res = iv;
                return res;
            }
            if (allDouble) {
                std::vector<double> dv;
                dv.reserve(out_values.size());
                for (auto const& e : out_values) dv.push_back(e.asDouble());
                res = dv;
                return res;
            }
            if (allString) {
                std::vector<std::string> sv;
                sv.reserve(out_values.size());
                for (auto const& e : out_values) sv.push_back(e.asString());
                res = sv;
                return res;
            }
            if (allBool) {
                std::vector<bool> bv;
                bv.reserve(out_values.size());
                for (auto const& e : out_values) bv.push_back(e.asBool());
                res = bv;
                return res;
            }
            if (allObject) {
                res = out_values;
                return res;
            }
            res = out_values;
            return res;
        }

        std::string parse_key() {
            skip_ws();
            if (peek() == '"') {
                auto v = parse_string();
                return v.asString();
            }
            // identifier key
            size_t start = i;
            while (std::isalnum(static_cast<unsigned char>(peek())) or peek() == '_') get();
            return s.substr(start, i - start);
        }

        Dictionary parse_object() {
            if (get() != '{') throw std::runtime_error("expected '{'");
            Dictionary d;
            skip_ws();
            if (peek() == '}') {
                get();
                return d;
            }
            while (true) {
                std::string key = parse_key();
                skip_ws();
                if (peek() == ':' or peek() == '=')
                    get();
                else {
                    size_t ctx_s = (i >= 20) ? i - 20 : 0;
                    size_t ctx_e = i + 20;
                    if (ctx_e > s.size()) ctx_e = s.size();
                    std::string snippet = s.substr(ctx_s, ctx_e - ctx_s);
                    std::ostringstream msg;
                    msg << "expected ':' or '=' after key near '" << snippet << "'";
                    throw std::runtime_error(msg.str());
                }
                skip_ws();
                Dictionary v = parse_value();
                // Detect duplicate keys and report an error
                if (d.m_object_map.find(key) != d.m_object_map.end()) {
                    throw std::runtime_error(std::string("duplicate key '") + key + "'");
                }
                d[key] = v;
                skip_ws();
                if (peek() == ',') {
                    get();
                    skip_ws();
                    if (peek() == '}') {
                        get();
                        break;
                    }
                    continue;
                }
                if (peek() == '}') {
                    get();
                    break;
                }
                // allow implicit separator
                continue;
            }
            return d;
        }

        Dictionary parse_value() {
            skip_ws();
            char c = peek();
            if (c == '{') return parse_object();
            if (c == '[') return parse_array();
            if (c == '"') return parse_string();
            if (std::isalpha(static_cast<unsigned char>(c)) or c == '_' or c == '-' or
                std::isdigit(static_cast<unsigned char>(c)))
                return parse_number_or_ident();
            {
                size_t ctx_s = (i >= 20) ? i - 20 : 0;
                size_t ctx_e = i + 20;
                if (ctx_e > s.size()) ctx_e = s.size();
                std::string snippet = s.substr(ctx_s, ctx_e - ctx_s);
                std::ostringstream msg;
                char pc = peek();
                if (pc == '\0')
                    msg << "unexpected end of input in RON";
                else
                    msg << "unexpected token in RON at index " << i << " ('" << pc << "') near '" << snippet << "'";
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
            if (p.peek() == ':' || p.peek() == '=')
                p.get();
            else
                throw std::runtime_error("expected ':' or '=' after key");
            p.skip_ws();
            Dictionary v = p.parse_value();
            // Detect duplicate keys in implicit root object
            if (root.m_object_map.find(key) != root.m_object_map.end()) {
                throw std::runtime_error(std::string("duplicate key '") + key + "'");
            }
            root[key] = v;
            p.skip_ws();
            if (p.peek() == ',') {
                p.get();
                p.skip_ws();
                continue;
            }
            // allow implicit separator
            if (std::isalpha(static_cast<unsigned char>(p.peek())) || p.peek() == '_' || p.peek() == '"') continue;
            break;
        }
        return root;
    }
    return p.parse_value();
}

}  // namespace ps
