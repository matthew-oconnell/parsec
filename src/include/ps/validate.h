#pragma once

#include <string>
#include <optional>
#include "ps/parsec.h"

namespace ps {

// Validate 'data' against a minimal schema represented as a Dictionary.
// Returns std::nullopt on success, or an error message on failure.
std::optional<std::string> validate(const Dictionary& data, const Dictionary& schema);

// Validate with line number tracking: accepts raw content to provide line numbers in errors
std::optional<std::string> validate(const Dictionary& data, const Dictionary& schema, const std::string& raw_content);

// Populate defaults: return a new Dictionary where missing properties
// that have a `default` in the schema are filled in. This is non-destructive
// (returns a new Dictionary). The function follows the same minimal schema
// conventions used by `validate` (supports `properties`, `additionalProperties`,
// nested object schemas, and `items` for arrays).
Dictionary setDefaults(const Dictionary& data, const Dictionary& schema);

}  // namespace ps
