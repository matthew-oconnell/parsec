#include <iostream>
#include <fstream>
#include <string>
#include <ps/simple_json.hpp>
#include <ps/ron.hpp>
#include <algorithm>

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "usage: parsec [--auto|--json|--ron] <file>\n";
        return 2;
    }
    std::string mode = "auto";
    std::string path;
    if (argc == 2) path = argv[1];
    else { mode = argv[1]; path = argv[2]; }
    std::ifstream in(path);
    if (!in) {
        std::cerr << "error: cannot open file: " << path << "\n";
        return 2;
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    try {
        auto ends_with = [](const std::string &s, const std::string &ext){ if (s.size() < ext.size()) return false; size_t off = s.size() - ext.size(); std::string se = s.substr(off); for (auto &c: se) c = static_cast<char>(::tolower(c)); std::string le = ext; for (auto &c: le) c = static_cast<char>(::tolower(c)); return se == le; };
        auto parse_with = [&](const std::string &which)->std::pair<bool, std::string> {
            try {
                if (which == "ron") { auto v = ps::parse_ron(content); return {true, v.to_string()}; }
                else { auto v = ps::parse_json(content); return {true, v.to_string()}; }
            } catch (const std::exception &e) { return {false, e.what()}; }
        };

        ps::Value v;
        bool parsed = false;
        std::string used;

        if (mode == "--json" || mode == "json") {
            auto r = parse_with("json");
            if (!r.first) throw std::runtime_error(std::string("JSON parse error: ") + r.second);
            parsed = true; used = "JSON";
            v = ps::parse_json(content);
        } else if (mode == "--ron" || mode == "ron") {
            auto r = parse_with("ron");
            if (!r.first) throw std::runtime_error(std::string("RON parse error: ") + r.second);
            parsed = true; used = "RON";
            v = ps::parse_ron(content);
        } else {
            // auto mode: pick by extension first, then fallback
            if (ends_with(path, ".ron")) {
                auto r = parse_with("ron");
                if (r.first) { parsed = true; used = "RON"; v = ps::parse_ron(content); }
                else {
                    auto r2 = parse_with("json");
                    if (r2.first) { parsed = true; used = "JSON"; v = ps::parse_json(content); }
                    else throw std::runtime_error(std::string("RON parse error: ") + r.second + "\n---\nJSON parse error: " + r2.second);
                }
            } else {
                auto r = parse_with("json");
                if (r.first) { parsed = true; used = "JSON"; v = ps::parse_json(content); }
                else {
                    auto r2 = parse_with("ron");
                    if (r2.first) { parsed = true; used = "RON"; v = ps::parse_ron(content); }
                    else throw std::runtime_error(std::string("JSON parse error: ") + r.second + "\n---\nRON parse error: " + r2.second);
                }
            }
        }
        auto shorten = [](const std::string &s, size_t n = 40){ if (s.size() <= n) return s; return s.substr(0,n-3) + "..."; };

        auto preview = [&](const ps::Value &val)->std::string {
            if (val.isDict()) {
                const auto &dptr = val.asDict();
                if (!dptr) return "{object, 0 keys}";
                size_t n = dptr->size();
                std::ostringstream ss; ss << "{object, " << n << " keys}";
                size_t shown = 0;
                for (auto const &p : dptr->items()) {
                    if (shown++ >= 3) break;
                    ss << (shown==1?" ":", ") << p.first << "=" << shorten(p.second.to_string());
                }
                if (n > 3) ss << ", ...";
                return ss.str();
            }
            if (val.isList()) {
                const auto &L = val.asList();
                std::ostringstream ss; ss << "[array, " << L.size() << " items]";
                size_t shown = 0;
                for (auto const &e : L) {
                    if (shown++ >= 3) break;
                    ss << (shown==1?" ":", ") << shorten(e.to_string());
                }
                if (L.size() > 3) ss << ", ...";
                return ss.str();
            }
            return val.to_string();
        };

    std::cout << "OK: parsed ";
    if (ends_with(path, ".ron")) std::cout << "RON"; else std::cout << "JSON";
    std::cout << "; value: " << v.to_string() << "\n";
    std::cout << "Preview: " << preview(v) << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::string what = e.what();
        // If the message already includes 'JSON parse error' or 'RON parse error', print as-is
        if (what.find("JSON parse error:") == 0 || what.find("RON parse error:") == 0) std::cerr << what << "\n";
        else std::cerr << "parse error: " << what << "\n";
        return 1;
    }
}
