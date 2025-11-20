#include <ps/parse.h>
#include <ps/json.h>
#include <ps/ron.h>
#include <ps/toml.h>
#include <ps/ini.h>
#include <ps/yaml.h>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace ps {

namespace {
    // Heuristic to estimate the intended format based on content
    std::string guess_format(const std::string& text) {
        // Skip leading whitespace
        size_t start = 0;
        while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
            ++start;
        }
        
        if (start >= text.size()) {
            return "JSON"; // Empty or whitespace-only, default to JSON
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
            ron_score += 2; // RON also uses these
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
                        if (k >= text.size() || text[k] == '\n' || text[k] == '\r' || text[k] == ';' || text[k] == '#') {
                            looks_like_ini_section = true;
                        }
                        break;
                    }
                    ++j;
                }
                if (looks_like_ini_section) {
                    ini_score += 2;
                    toml_score += 1; // TOML also has sections
                }
            }

            // TOML table arrays [[table]]
            if (i + 1 < text.size() && text[i] == '[' && text[i + 1] == '[') {
                toml_score += 3;
            }

            // YAML indicators
            if (i == 0 || text[i - 1] == '\n') {
                if (text[i] == '-' && i + 1 < text.size() && text[i + 1] == ' ') {
                    yaml_score += 2; // List item
                }
                // YAML key: value at start of line
                size_t colon_pos = i;
                while (colon_pos < text.size() && text[colon_pos] != '\n' && text[colon_pos] != ':') {
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
            if (text[i] == '=' && (i == 0 || (text[i - 1] != '=' && text[i - 1] != '!' && text[i - 1] != '<' && text[i - 1] != '>'))) {
                ini_score += 1;
                toml_score += 1;
            }
        }

        // Determine most likely format
        int max_score = std::max({json_score, ini_score, toml_score, yaml_score, ron_score});
        
        if (max_score == 0) {
            return "JSON"; // Default
        }
        
        if (json_score == max_score) return "JSON";
        if (toml_score == max_score) return "TOML";
        if (ini_score == max_score) return "INI";
        if (yaml_score == max_score) return "YAML";
        if (ron_score == max_score) return "RON";
        
        return "JSON"; // Fallback
    }
}

Dictionary parse(const std::string& text) {
    std::string json_error, ron_error, toml_error, ini_error, yaml_error;
    
    // Try JSON first (most strict)
    try {
        return parse_json(text);
    } catch (const std::exception& e) {
        json_error = e.what();
    }

    // Try RON (relaxed JSON)
    try {
        return parse_ron(text);
    } catch (const std::exception& e) {
        ron_error = e.what();
    }

    // Try TOML
    try {
        return parse_toml(text);
    } catch (const std::exception& e) {
        toml_error = e.what();
    }

    // Try INI
    try {
        return parse_ini(text);
    } catch (const std::exception& e) {
        ini_error = e.what();
    }

    // Try YAML
    try {
        return parse_yaml(text);
    } catch (const std::exception& e) {
        yaml_error = e.what();
    }

    // All parsers failed - guess the intended format and report that error
    std::string guessed_format = guess_format(text);
    
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