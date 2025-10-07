// Simple CLI for parsing a JSON file using parsec
#include <iostream>
#include <fstream>
#include <string>
#include <ps/simple_json.hpp>
#include <algorithm>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: parsec_cli <file.json>\n";
        return 2;
    }
    const std::string path = argv[1];
    std::ifstream in(path);
    if (!in) {
        std::cerr << "error: cannot open file: " << path << "\n";
        return 2;
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    try {
        auto v = ps::parse_json(content);
        auto shorten = [](const std::string &s, size_t n = 40){ if (s.size() <= n) return s; return s.substr(0,n-3) + "..."; };

        auto preview = [&](const ps::Value &val)->std::string {
            if (val.is_dict()) {
                const auto &dptr = val.as_dict();
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
            if (val.is_list()) {
                const auto &L = val.as_list();
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

        std::cout << "OK: parsed JSON; value: " << v.to_string() << "\n";
        std::cout << "Preview: " << preview(v) << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "parse error: " << e.what() << "\n";
        return 1;
    }
}
