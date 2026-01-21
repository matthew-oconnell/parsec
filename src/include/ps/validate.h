#pragma once

#include <string>
#include <optional>
#include <vector>
#include "ps/parsec.h"

namespace ps {

// Error severity levels
enum class ErrorSeverity {
    ERROR,       // Validation failures (type mismatch, missing required, etc.)
    WARNING,     // Non-critical issues (e.g., extra properties when additionalProperties: false)
    DEPRECATION  // Deprecated property/value usage
};

// Error category for classification
enum class ErrorCategory {
    MISSING_REQUIRED,
    TYPE_MISMATCH,
    OUT_OF_RANGE,
    INVALID_ENUM,
    DEPRECATED_PROPERTY,
    DEPRECATED_VALUE,
    ADDITIONAL_PROPERTY,
    PATTERN_MISMATCH,
    ARRAY_SIZE,
    UNIQUE_ITEMS,
    ONE_OF_MISMATCH,
    ANY_OF_MISMATCH,
    ALL_OF_FAILURE,
    OTHER
};

// Individual validation error
struct ValidationError {
    std::string path;     // e.g., "cfd/boundary conditions/0/type"
    std::string message;  // Human-readable error message
    int line_number;      // Line in source file (-1 if unknown)
    int depth;            // Depth in JSON tree (0 = root)
    ErrorSeverity severity;
    ErrorCategory category;

    ValidationError(const std::string& p,
                    const std::string& msg,
                    int line,
                    int d,
                    ErrorSeverity sev,
                    ErrorCategory cat)
        : path(p), message(msg), line_number(line), depth(d), severity(sev), category(cat) {}
};

// Result of validation containing all errors found
struct ValidationResult {
    std::vector<ValidationError> errors;

    // Check if validation passed (no errors at all)
    bool is_valid() const { return errors.empty(); }

    // Get count by severity
    int error_count() const;
    int warning_count() const;
    int deprecation_count() const;

    // Basic formatter - returns formatted string of all errors
    std::string format() const;
};

// New primary API - returns all validation errors
ValidationResult validate_all(const Dictionary& data,
                              const Dictionary& schema,
                              const std::string& raw_content = "");

// Set schema context for better error messages (optional)
// Call this before validate_all to provide schema file information in error messages
void set_schema_context(const std::string& filename, const std::string& content);

// Set data filename for better error messages (optional)
void set_data_filename(const std::string& filename);

// Backward compatible API - returns first error message
// Returns std::nullopt on success, or an error message on failure.
std::optional<std::string> validate(const Dictionary& data, const Dictionary& schema);

// Validate with line number tracking: accepts raw content to provide line numbers in errors
std::optional<std::string> validate(const Dictionary& data,
                                    const Dictionary& schema,
                                    const std::string& raw_content);

// Populate defaults: return a new Dictionary where missing properties
// that have a `default` in the schema are filled in. This is non-destructive
// (returns a new Dictionary). The function follows the same minimal schema
// conventions used by `validate` (supports `properties`, `additionalProperties`,
// nested object schemas, and `items` for arrays).
Dictionary setDefaults(const Dictionary& data, const Dictionary& schema);

}  // namespace ps
