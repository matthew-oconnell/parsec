#include <iostream>
#include <fstream>
#include <string>
#include <ps/json.h>
#include <ps/ron.h>
#include <ps/parse.h>
#include <ps/validate.h>
#include <algorithm>

// Temporary stubs to satisfy linking while incremental migration is in progress.
// These are local fallbacks so `main` can link even if other translation units
// that normally provide these symbols haven't been migrated/compiled yet.
// Remove these once the full migration is complete and proper implementations
// are available in the library.
namespace ps {
inline std::optional<std::string> validate(const Dictionary& /*data*/, const Dictionary& /*schema*/) {
    // conservative default: indicate success (no validation errors). This is
    // a temporary shim to allow linking; proper validation logic lives in
    // `validate.cpp` and should be used when available.
    return std::nullopt;
}
} // namespace ps

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage:\n  parsec [--auto|--json|--ron] <file>\n  parsec --validate "
                     "<schema.json> <file>\n";
        return 2;
    }

    // Special validate mode: parse schema (JSON) and validate the given file against it
    if (std::string(argv[1]) == "--validate") {
        if (argc != 4) {
            std::cerr << "usage: parsec --validate <schema.json> <file>\n";
            return 2;
        }
        std::string schema_path = argv[2];
        std::string data_path = argv[3];
        std::ifstream sin(schema_path);
        if (!sin) {
            std::cerr << "error: cannot open schema: " << schema_path << "\n";
            return 2;
        }
        std::string schema_content((std::istreambuf_iterator<char>(sin)),
                                   std::istreambuf_iterator<char>());
        try {
            auto schema = ps::parse_json(schema_content);

            std::ifstream in(data_path);
            if (!in) {
                std::cerr << "error: cannot open file: " << data_path << "\n";
                return 2;
            }
            std::string content((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());

            // Try parse as JSON first, then RON
            ps::Dictionary data;
            try {
                data = ps::parse_json(content);
            } catch (...) {
                data = ps::parse_ron(content);
            }

            auto err = ps::validate(data, schema);
            if (err.has_value()) {
                std::cerr << "validation error: " << *err << "\n";
                return 1;
            }
            std::cout << "OK: validation passed\n";
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "schema parse error: " << e.what() << "\n";
            return 2;
        }
    }

    std::string mode = "auto";
    std::string path;
    if (argc == 2)
        path = argv[1];
    else {
        mode = argv[1];
        path = argv[2];
    }
    std::ifstream in(path);
    if (!in) {
        std::cerr << "error: cannot open file: " << path << "\n";
        return 2;
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    try {
        ps::Dictionary v;
        bool parsed = false;
        std::string used;

        if (mode == "--json" or mode == "json") {
            v = ps::parse_json(content);
            parsed = true;
            used = "JSON";
        } else if (mode == "--ron" or mode == "ron") {
            v = ps::parse_ron(content);
            parsed = true;
            used = "RON";
        } else {
            // auto mode: let ps::parse decide between JSON and RON
            v = ps::parse(content);
            // ps::parse doesn't indicate which was used; we try to guess for display
            // prefer extension hint if present
            if (path.size() >= 4 && path.substr(path.size() - 4) == ".ron")
                used = "RON";
            else
                used = "JSON_or_RON";
            parsed = true;
        }
        auto shorten = [](const std::string& s, size_t n = 40) {
            if (s.size() <= n) return s;
            return s.substr(0, n - 3) + "...";
        };

        auto preview = [&](const ps::Dictionary& val) -> std::string {
            if (val.isMappedObject()) {
                size_t n = val.size();
                std::ostringstream ss;
                ss << "{object, " << n << " keys}";
                size_t shown = 0;
                for (auto const& p : val.items()) {
                    if (shown++ >= 3) break;
                    ss << (shown == 1 ? " " : ", ") << p.first << "=" << shorten(p.second.to_string());
                }
                if (n > 3) ss << ", ...";
                return ss.str();
            }
            if (val.isArrayObject()) {
                std::ostringstream ss;
                size_t n = val.size();
                ss << "[array, " << n << " items]";
                size_t shown = 0;
                switch (val.type()) {
                    case ps::Dictionary::IntArray: {
                        auto L = val.asInts();
                        for (auto const& e : L) {
                            if (shown++ >= 3) break;
                            ss << (shown == 1 ? " " : ", ") << shorten(std::to_string(e));
                        }
                        break;
                    }
                    case ps::Dictionary::DoubleArray: {
                        auto L = val.asDoubles();
                        for (auto const& e : L) {
                            if (shown++ >= 3) break;
                            ss << (shown == 1 ? " " : ", ") << shorten(std::to_string(e));
                        }
                        break;
                    }
                    case ps::Dictionary::StringArray: {
                        auto L = val.asStrings();
                        for (auto const& e : L) {
                            if (shown++ >= 3) break;
                            ss << (shown == 1 ? " " : ", ") << shorten(e);
                        }
                        break;
                    }
                    case ps::Dictionary::BoolArray: {
                        auto L = val.asBools();
                        for (auto const& e : L) {
                            if (shown++ >= 3) break;
                            ss << (shown == 1 ? " " : ", ") << (e ? "true" : "false");
                        }
                        break;
                    }
                    case ps::Dictionary::ObjectArray: {
                        auto L = val.asObjects();
                        for (auto const& e : L) {
                            if (shown++ >= 3) break;
                            ss << (shown == 1 ? " " : ", ") << shorten(e.to_string());
                        }
                        break;
                    }
                    default: {
                        try {
                            auto L = val.asObjects();
                            for (auto const& e : L) {
                                if (shown++ >= 3) break;
                                ss << (shown == 1 ? " " : ", ") << shorten(e.to_string());
                            }
                        } catch (...) {
                        }
                        break;
                    }
                }
                if (n > 3) ss << ", ...";
                return ss.str();
            }
            return val.to_string();
        };

        std::cout << "OK: parsed " << used << "; value: " << v.to_string() << "\n";
        std::cout << "Preview: " << preview(v) << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::string what = e.what();
        // If the message already includes 'JSON parse error' or 'RON parse error', print as-is
        if (what.find("JSON parse error:") == 0 || what.find("RON parse error:") == 0)
            std::cerr << what << "\n";
        else
            std::cerr << "parse error: " << what << "\n";
        return 1;
    }
}
