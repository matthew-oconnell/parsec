// Simple in-repo JSON parser API (moved under src/include)
#pragma once

#include <ps/dictionary.hpp>
#include <string>

namespace ps {

Value parse_json(const std::string& text);

} // namespace ps
