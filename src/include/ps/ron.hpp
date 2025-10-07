// Simple RON (Rusty Object Notation) parser API
#pragma once

#include <ps/dictionary.hpp>
#include <string>

namespace ps {

// Parse a RON-formatted string into a ps::Value. RON here is a small,
// permissive superset of JSON that allows unquoted identifier keys and
// either ':' or '=' as a key/value separator. This is intentionally
// lightweight and not a full RON implementation.
Value parse_ron(const std::string& text);

} // namespace ps
