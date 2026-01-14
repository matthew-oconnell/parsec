#pragma once

#include <ps/dictionary.h>
#include <string>
#include <utility>

namespace ps {

// Generic parser that auto-detects JSON vs RON. It will try JSON first,
// then fall back to RON. If both fail it throws a runtime_error with
// both parser error messages joined.
// When `verbose` is true, the router prints which format hints were found,
// which parsers were attempted (and their errors), and which parser succeeded.
// If `filename` is provided, the file extension is used to determine the parser.
Dictionary parse(const std::string& text, bool verbose = false, const std::string& filename = "");

// Same as parse() but returns a pair containing the Dictionary and the format name used.
// Format names: "JSON", "RON", "TOML", "INI", "YAML"
std::pair<Dictionary, std::string> parse_report_format(const std::string& text, bool verbose = false, const std::string& filename = "");

Dictionary parse_json(const std::string& text);
Dictionary parse_ron(const std::string& text);
Dictionary parse_toml(const std::string& text);
Dictionary parse_ini(const std::string& text);

}  // namespace ps