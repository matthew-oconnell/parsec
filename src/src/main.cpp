#include <iostream>
#include <fstream>
#include <string>
#include <ps/json.h>
#include <ps/ron.h>
#include <ps/toml.h>
#include <ps/ini.h>
#include <ps/yaml.h>
#include <ps/parse.h>
#include <ps/validate.h>
#include <algorithm>

// The real `ps::validate` implementation is provided in `validate.cpp`.
// Do not provide a local stub here so the CLI calls the library implementation.

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage:\n  parsec [--auto|--json|--ron|--toml|--ini] <file>\n  parsec "
                     "--validate [--no-defaults] <schema.json> <file>\n  parsec --fill-defaults "
                     "<schema.json> <input> <output>\n  parsec --convert "
                     "<yaml|json|ron|toml> <input> <output>\n";
        return 2;
    }

    // Special validate mode: parse schema (JSON) and validate the given file against it
    if (std::string(argv[1]) == "--validate") {
        bool apply_defaults = true;
        int schema_idx = 2;
        int data_idx = 3;
        int expected_argc = 4;

        // Check for --no-defaults flag
        if (argc >= 3 && std::string(argv[2]) == "--no-defaults") {
            apply_defaults = false;
            schema_idx = 3;
            data_idx = 4;
            expected_argc = 5;
        }

        if (argc != expected_argc) {
            std::cerr << "usage: parsec --validate [--no-defaults] <schema.json> <file>\n";
            return 2;
        }
        std::string schema_path = argv[schema_idx];
        std::string data_path = argv[data_idx];
        std::ifstream sin(schema_path);
        if (!sin) {
            std::cerr << "error: cannot open schema: " << schema_path << "\n";
            return 2;
        }
        std::string schema_content((std::istreambuf_iterator<char>(sin)),
                                   std::istreambuf_iterator<char>());
        try {
            auto schema = ps::parse(schema_content);

            std::ifstream in(data_path);
            if (!in) {
                std::cerr << "error: cannot open file: " << data_path << "\n";
                return 2;
            }
            std::string content((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());

            ps::Dictionary data = ps::parse(content);

            // Apply defaults from schema if requested
            if (apply_defaults) {
                data = ps::setDefaults(data, schema);
            }

            auto result = ps::validate_all(data, schema, content);
            if (!result.is_valid()) {
                std::cerr << result.format();
                return 1;
            }
            std::cout << "OK: validation passed\n";
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "schema parse error: " << e.what() << "\n";
            return 2;
        }
    }

    // Fill defaults mode: --fill-defaults <schema.json> <input> <output>
    if (std::string(argv[1]) == "--fill-defaults") {
        if (argc != 5) {
            std::cerr << "usage: parsec --fill-defaults <schema.json> <input> <output>\n";
            return 2;
        }
        std::string schema_path = argv[2];
        std::string data_path = argv[3];
        std::string out_path = argv[4];

        std::ifstream sin(schema_path);
        if (!sin) {
            std::cerr << "error: cannot open schema: " << schema_path << "\n";
            return 2;
        }
        std::string schema_content((std::istreambuf_iterator<char>(sin)),
                                   std::istreambuf_iterator<char>());

        std::ifstream in(data_path);
        if (!in) {
            std::cerr << "error: cannot open file: " << data_path << "\n";
            return 2;
        }
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

        try {
            auto schema = ps::parse(schema_content);
            ps::Dictionary data = ps::parse(content);

            ps::Dictionary completed = ps::setDefaults(data, schema);

            std::ofstream out_file(out_path);
            if (!out_file) {
                std::cerr << "error: cannot open output: " << out_path << "\n";
                return 2;
            }
            // Write pretty JSON with indentation
            out_file << completed.dump(4, false) << std::endl;
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
            return 1;
        }
    }

    // Convert mode: --convert <yaml|json|ron|toml> <input> <output>
    if (std::string(argv[1]) == "--convert") {
        if (argc != 5) {
            std::cerr << "usage: parsec --convert <yaml|json|ron|toml> <input> <output>\n";
            return 2;
        }
        std::string fmt = argv[2];
        std::string in_path = argv[3];
        std::string out_path = argv[4];

        std::ifstream in_file(in_path);
        if (!in_file) {
            std::cerr << "error: cannot open file: " << in_path << "\n";
            return 2;
        }
        std::string content((std::istreambuf_iterator<char>(in_file)),
                            std::istreambuf_iterator<char>());

        try {
            ps::Dictionary data = ps::parse(content, false, in_path);

            std::ofstream out_file(out_path);
            if (!out_file) {
                std::cerr << "error: cannot open output: " << out_path << "\n";
                return 2;
            }

            if (fmt == "yaml" || fmt == "--yaml") {
                out_file << ps::dump_yaml(data);
            } else if (fmt == "json" || fmt == "--json") {
                // Use Dictionary::dump with indentation for readable JSON
                out_file << data.dump(4, false) << std::endl;
            } else if (fmt == "ron" || fmt == "--ron") {
                out_file << ps::dump_ron(data);
            } else if (fmt == "toml" || fmt == "--toml") {
                out_file << ps::dump_toml(data);
            } else {
                std::cerr << "error: unknown format '" << fmt
                          << "' (expected 'yaml', 'json', 'ron' or 'toml')\n";
                return 2;
            }

            return 0;
        } catch (const std::exception& e) {
            std::cerr << "parse error: " << e.what() << "\n";
            return 1;
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
        std::string used;

        if (mode == "--json" or mode == "json") {
            v = ps::parse_json(content);
            used = "JSON";
        } else if (mode == "--ron" or mode == "ron") {
            v = ps::parse_ron(content);
            used = "RON";
        } else if (mode == "--toml" or mode == "toml") {
            v = ps::parse_toml(content);
            used = "TOML";
        } else if (mode == "--ini" or mode == "ini") {
            v = ps::parse_ini(content);
            used = "INI";
        } else {
            // auto mode: let ps::parse decide based on filename and content
            auto [dict, format] = ps::parse_report_format(content, false, path);
            v = dict;
            used = format;
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
                    ss << (shown == 1 ? " " : ", ") << p.first << "="
                       << shorten(p.second.to_string());
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

        std::cout << "OK: parsed as " << used << "\n";
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
