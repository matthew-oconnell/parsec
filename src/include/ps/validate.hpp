#pragma once

#include <string>
#include <optional>
#include "ps/parsec.hpp"

namespace ps {

// Validate 'data' against a minimal schema represented as a Dictionary.
// Returns std::nullopt on success, or an error message on failure.
std::optional<std::string> validate(const Dictionary& data, const Dictionary& schema);

} // namespace ps
