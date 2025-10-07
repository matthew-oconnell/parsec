// Simple in-repo JSON parser (no external dependencies)
#pragma once

#include <ps/dictionary.hpp>
#include <string>

namespace ps {

// Parse a JSON string into a ps::Value. Throws std::runtime_error on errors.
Value parse_json(const std::string& text);

} // namespace ps
