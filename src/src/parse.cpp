#include <ps/parse.h>
#include <ps/json.h>
#include <ps/ron.h>
#include <ps/toml.h>
#include <ps/ini.h>
#include <ps/yaml.h>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <regex>
#include <iostream>
#include <vector>
#include <map>

namespace ps {

namespace {
    // Extract format hint from first line comment if present
    // Supports: vim modeline, emacs mode line, and simple "format:" pragma
    // Examples:
    //   # vim: set filetype=json:
    //   # vim: ft=toml
    //   # -*- mode: yaml -*-
    //   # format: ini
    std::string extract_format_hint(const std::string& text) {
        // Only check first line
        size_t first_newline = text.find('\n');
        std::string first_line =
                    (first_newline != std::string::npos) ? text.substr(0, first_newline) : text;

        // Skip if first line doesn't start with a comment character
        size_t start = 0;
        while (start < first_line.size() &&
               std::isspace(static_cast<unsigned char>(first_line[start]))) {
            ++start;
        }
        if (start >= first_line.size() || (first_line[start] != '#' && first_line[start] != ';')) {
            return "";  // No comment on first line
        }

        // Convert to lowercase for case-insensitive matching
        std::string lower_line = first_line;
        std::transform(
                    lower_line.begin(), lower_line.end(), lower_line.begin(), [](unsigned char c) {
                        return std::tolower(c);
                    });

        // Try vim modeline: "vim: set filetype=X:" or "vim: ft=X"
        std::regex vim_pattern(R"(vim:\s*(?:set\s+)?(?:filetype|ft)\s*=\s*(\w+))");
        std::smatch match;
        if (std::regex_search(lower_line, match, vim_pattern)) {
            return match[1].str();
        }

        // Try emacs mode line: "-*- mode: X -*-"
        std::regex emacs_pattern(R"(-\*-\s*mode:\s*(\w+)\s*-\*-)");
        if (std::regex_search(lower_line, match, emacs_pattern)) {
            return match[1].str();
        }

        // Try simple format pragma: "format: X"
        std::regex format_pattern(R"(format:\s*(\w+))");
        if (std::regex_search(lower_line, match, format_pattern)) {
            return match[1].str();
        }

        return "";  // No format hint found
    }

    // Heuristic to estimate the intended format based on content
    std::string guess_format(const std::string& text) {
        // Skip leading whitespace
        size_t start = 0;
        while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
            ++start;
        }

        if (start >= text.size()) {
            return "JSON";  // Empty or whitespace-only, default to JSON
        }

        // Count format indicators
        int json_score = 0;
        int ini_score = 0;
        int toml_score = 0;
        int yaml_score = 0;
        int ron_score = 0;

        char first_char = text[start];

        // JSON typically starts with { or [
        if (first_char == '{' || first_char == '[') {
            json_score += 3;
            ron_score += 2;  // RON also uses these
        }

        // Look for format-specific patterns
        for (size_t i = 0; i < text.size(); ++i) {
            // INI section headers [section]
            if (text[i] == '[' && i + 1 < text.size()) {
                size_t j = i + 1;
                bool looks_like_ini_section = false;
                while (j < text.size() && text[j] != '\n') {
                    if (text[j] == ']') {
                        // Check if it's followed by newline or end (INI pattern)
                        size_t k = j + 1;
                        while (k < text.size() && (text[k] == ' ' || text[k] == '\t')) ++k;
                        if (k >= text.size() || text[k] == '\n' || text[k] == '\r' ||
                            text[k] == ';' || text[k] == '#') {
                            looks_like_ini_section = true;
                        }
                        break;
                    }
                    ++j;
                }
                if (looks_like_ini_section) {
                    ini_score += 2;
                    toml_score += 1;  // TOML also has sections
                }
            }

            // TOML table arrays [[table]]
            if (i + 1 < text.size() && text[i] == '[' && text[i + 1] == '[') {
                toml_score += 3;
            }

            // YAML indicators
            if (i == 0 || text[i - 1] == '\n') {
                if (text[i] == '-' && i + 1 < text.size() && text[i + 1] == ' ') {
                    yaml_score += 2;  // List item
                }
                // YAML key: value at start of line
                size_t colon_pos = i;
                while (colon_pos < text.size() && text[colon_pos] != '\n' &&
                       text[colon_pos] != ':') {
                    ++colon_pos;
                }
                if (colon_pos < text.size() && text[colon_pos] == ':' &&
                    colon_pos + 1 < text.size() && text[colon_pos + 1] == ' ') {
                    yaml_score += 1;
                }
            }

            // Comments
            if (text[i] == '#') {
                yaml_score += 1;
                toml_score += 1;
                ini_score += 1;
            }
            if (text[i] == ';') {
                ini_score += 1;
            }
            if (i + 1 < text.size() && text[i] == '/' && text[i + 1] == '/') {
                ron_score += 2;
            }

            // JSON-specific: lots of quoted keys
            if (text[i] == '"') {
                size_t j = i + 1;
                while (j < text.size() && text[j] != '"' && text[j] != '\n') {
                    if (text[j] == '\\') ++j;
                    ++j;
                }
                if (j < text.size() && text[j] == '"') {
                    // Check if followed by colon (JSON object key)
                    size_t k = j + 1;
                    while (k < text.size() && (text[k] == ' ' || text[k] == '\t')) ++k;
                    if (k < text.size() && text[k] == ':') {
                        json_score += 1;
                    }
                }
            }

            // INI/TOML key=value
            if (text[i] == '=' && (i == 0 || (text[i - 1] != '=' && text[i - 1] != '!' &&
                                              text[i - 1] != '<' && text[i - 1] != '>'))) {
                ini_score += 1;
                toml_score += 1;
            }
        }

        // Determine most likely format
        int max_score = std::max({json_score, ini_score, toml_score, yaml_score, ron_score});

        if (max_score == 0) {
            return "JSON";  // Default
        }

        if (json_score == max_score) return "JSON";
        if (toml_score == max_score) return "TOML";
        if (ini_score == max_score) return "INI";
        if (yaml_score == max_score) return "YAML";
        if (ron_score == max_score) return "RON";

        return "JSON";  // Fallback
    }

    // Extract format from filename extension
    std::string format_from_filename(const std::string& filename) {
        if (filename.empty()) return "";
        
        size_t dot_pos = filename.rfind('.');
        if (dot_pos == std::string::npos || dot_pos == filename.size() - 1) {
            return "";  // No extension
        }
        
        std::string ext = filename.substr(dot_pos + 1);
        // Convert to lowercase
        std::transform(ext.begin(), ext.end(), ext.begin(), 
                      [](unsigned char c) { return std::tolower(c); });
        
        if (ext == "json") return "json";
        if (ext == "ron") return "ron";
        if (ext == "toml") return "toml";
        if (ext == "ini") return "ini";
        if (ext == "yaml" || ext == "yml") return "yaml";
        
        return "";  // Unknown extension
    }
}

Dictionary parse(const std::string& text, bool verbose, const std::string& filename) {
    auto [dict, format] = parse_report_format(text, verbose, filename);
    return dict;
}

std::pair<Dictionary, std::string> parse_report_format(const std::string& text, bool verbose, const std::string& filename) {
    // Track attempts and errors to support verbose reporting
    std::vector<std::string> attempted_parsers;
    std::map<std::string, std::string> parser_errors;

    // Check for filename-based format hint first
    std::string hint = format_from_filename(filename);
    if (!hint.empty() && verbose) {
        std::cerr << "Format hint from filename: '" << hint << "'\n";
    }
    
    // Check for explicit format hint in first line comment (overrides filename)
    std::string content_hint = extract_format_hint(text);
    if (!content_hint.empty()) {
        hint = content_hint;
    }
    if (!hint.empty()) {
        if (verbose) std::cerr << "Format hint found: '" << hint << "'\n";
        // If user provided an explicit hint, use ONLY that parser
        // and report errors from that format (when verbose)
        try {
            if (hint == "json") {
                attempted_parsers.emplace_back("JSON");
                Dictionary d = parse_json(text);
                if (verbose) std::cerr << "Used parser: JSON (from hint)\n";
                return {d, "JSON"};
            }
            if (hint == "ron") {
                attempted_parsers.emplace_back("RON");
                Dictionary d = parse_ron(text);
                if (verbose) std::cerr << "Used parser: RON (from hint)\n";
                return {d, "RON"};
            }
            if (hint == "toml") {
                attempted_parsers.emplace_back("TOML");
                Dictionary d = parse_toml(text);
                if (verbose) std::cerr << "Used parser: TOML (from hint)\n";
                return {d, "TOML"};
            }
            if (hint == "ini") {
                attempted_parsers.emplace_back("INI");
                Dictionary d = parse_ini(text);
                if (verbose) std::cerr << "Used parser: INI (from hint)\n";
                return {d, "INI"};
            }
            if (hint == "yaml" || hint == "yml") {
                attempted_parsers.emplace_back("YAML");
                Dictionary d = parse_yaml(text);
                if (verbose) std::cerr << "Used parser: YAML (from hint)\n";
                return {d, "YAML"};
            }
            // If hint is unrecognized, fall through to normal auto-detection
            if (verbose)
                std::cerr << "Unrecognized hint '" << hint
                          << "' - falling back to auto-detection\n";
        } catch (const std::exception& e) {
            parser_errors[hint] = e.what();
            if (verbose)
                std::cerr << "Parser error for hint '" << hint << "': " << e.what() << "\n";
            // Re-throw to preserve original behavior for hinted parse failures
            throw;
        }
    }

    // Try JSON first (most strict)
    try {
        attempted_parsers.emplace_back("JSON");
        Dictionary d = parse_json(text);
        if (verbose) std::cerr << "Attempted parsers: JSON => success\n";
        if (verbose) std::cerr << "Used parser: JSON\n";
        return {d, "JSON"};
    } catch (const std::exception& e) {
        parser_errors["JSON"] = e.what();
    }

    // Try RON (relaxed JSON)
    try {
        attempted_parsers.emplace_back("RON");
        Dictionary d = parse_ron(text);
        if (verbose) std::cerr << "Attempted parsers: RON => success\n";
        if (verbose) std::cerr << "Used parser: RON\n";
        return {d, "RON"};
    } catch (const std::exception& e) {
        parser_errors["RON"] = e.what();
    }

    // Try TOML
    try {
        attempted_parsers.emplace_back("TOML");
        Dictionary d = parse_toml(text);
        if (verbose) std::cerr << "Attempted parsers: TOML => success\n";
        if (verbose) std::cerr << "Used parser: TOML\n";
        return {d, "TOML"};
    } catch (const std::exception& e) {
        parser_errors["TOML"] = e.what();
    }

    // Try YAML
    try {
        attempted_parsers.emplace_back("YAML");
        Dictionary d = parse_yaml(text);
        if (verbose) std::cerr << "Attempted parsers: YAML => success\n";
        if (verbose) std::cerr << "Used parser: YAML\n";
        return {d, "YAML"};
    } catch (const std::exception& e) {
        parser_errors["YAML"] = e.what();
    }

    // Try INI
    try {
        attempted_parsers.emplace_back("INI");
        Dictionary d = parse_ini(text);
        if (verbose) std::cerr << "Attempted parsers: INI => success\n";
        if (verbose) std::cerr << "Used parser: INI\n";
        return {d, "INI"};
    } catch (const std::exception& e) {
        parser_errors["INI"] = e.what();
    }

    // All parsers failed - guess the intended format and report that error
    std::string guessed_format = guess_format(text);

    if (verbose) {
        std::cerr << "All parsers attempted. Summary:\n";
        for (const auto& name : attempted_parsers) {
            auto it = parser_errors.find(name);
            if (it != parser_errors.end()) {
                std::cerr << " - " << name << ": error: " << it->second << "\n";
            } else {
                std::cerr << " - " << name << ": success\n";
            }
        }
        for (const auto& p : parser_errors) {
            if (std::find(attempted_parsers.begin(), attempted_parsers.end(), p.first) ==
                attempted_parsers.end()) {
                std::cerr << " - " << p.first << ": error: " << p.second << "\n";
            }
        }
        std::cerr << "Most likely intended format: " << guessed_format << "\n";
    }

    std::string json_error = parser_errors.count("JSON") ? parser_errors["JSON"] : std::string();
    std::string ron_error = parser_errors.count("RON") ? parser_errors["RON"] : std::string();
    std::string toml_error = parser_errors.count("TOML") ? parser_errors["TOML"] : std::string();
    std::string ini_error = parser_errors.count("INI") ? parser_errors["INI"] : std::string();
    std::string yaml_error = parser_errors.count("YAML") ? parser_errors["YAML"] : std::string();

    std::string error_msg = "Failed to parse as any supported format. ";
    error_msg += "Most likely intended format: " + guessed_format + "\n\n";

    if (guessed_format == "JSON" && !json_error.empty()) {
        error_msg += json_error;
    } else if (guessed_format == "RON" && !ron_error.empty()) {
        error_msg += ron_error;
    } else if (guessed_format == "TOML" && !toml_error.empty()) {
        error_msg += toml_error;
    } else if (guessed_format == "INI" && !ini_error.empty()) {
        error_msg += ini_error;
    } else if (guessed_format == "YAML" && !yaml_error.empty()) {
        error_msg += yaml_error;
    } else {
        // Fallback: show all errors
        error_msg += "All parser errors:\n";
        error_msg += "JSON: " + json_error + "\n";
        error_msg += "RON: " + ron_error + "\n";
        error_msg += "TOML: " + toml_error + "\n";
        error_msg += "INI: " + ini_error + "\n";
        error_msg += "YAML: " + yaml_error;
    }

    throw std::runtime_error(error_msg);
}

}  // namespace ps