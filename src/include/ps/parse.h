#pragma once

#include <ps/dictionary.h>
#include <string>

namespace ps {

// Generic parser that auto-detects JSON vs RON. It will try JSON first,
// then fall back to RON. If both fail it throws a runtime_error with
// both parser error messages joined.
Dictionary parse(const std::string& text);

Dictionary parse_json(const std::string& text);
Dictionary parse_ron(const std::string& text);

} // namespace ps