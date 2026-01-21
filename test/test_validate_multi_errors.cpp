#include <catch2/catch_test_macros.hpp>
#include <ps/parsec.h>
#include <ps/validate.h>

using namespace ps;

TEST_CASE("validate_all collects multiple missing required errors", "[validate][multi-error]") {
    // Schema requires 'name', 'port', and 'host'
    Dictionary schema = parse_json(R"({
        "type": "object",
        "required": ["name", "port", "host"],
        "properties": {
            "name": {"type": "string"},
            "port": {"type": "integer"},
            "host": {"type": "string"}
        }
    })");

    // Data only has 'port', missing both 'name' and 'host'
    Dictionary data = parse_json(R"({
        "port": 8080
    })");

    auto result = validate_all(data, schema);

    REQUIRE_FALSE(result.is_valid());
    REQUIRE(result.errors.size() == 2);
    REQUIRE(result.error_count() == 2);
    REQUIRE(result.deprecation_count() == 0);

    // Check that both missing keys are reported
    bool found_name = false;
    bool found_host = false;
    for (const auto& err : result.errors) {
        if (err.message.find("missing required key 'name'") != std::string::npos) {
            found_name = true;
            REQUIRE(err.severity == ErrorSeverity::ERROR);
            REQUIRE(err.category == ErrorCategory::MISSING_REQUIRED);
        }
        if (err.message.find("missing required key 'host'") != std::string::npos) {
            found_host = true;
            REQUIRE(err.severity == ErrorSeverity::ERROR);
            REQUIRE(err.category == ErrorCategory::MISSING_REQUIRED);
        }
    }
    REQUIRE(found_name);
    REQUIRE(found_host);
}

TEST_CASE("validate_all collects deprecation warnings", "[validate][multi-error][deprecation]") {
    // Schema with deprecated property
    Dictionary schema = parse_json(R"({
        "type": "object",
        "properties": {
            "old_name": {
                "type": "string",
                "deprecated": true,
                "description": "Use 'new_name' instead"
            },
            "new_name": {"type": "string"}
        }
    })");

    Dictionary data = parse_json(R"({
        "old_name": "value"
    })");

    auto result = validate_all(data, schema);

    REQUIRE_FALSE(result.is_valid());
    REQUIRE(result.errors.size() == 1);
    REQUIRE(result.error_count() == 0);
    REQUIRE(result.deprecation_count() == 1);

    const auto& err = result.errors[0];
    REQUIRE(err.severity == ErrorSeverity::DEPRECATION);
    REQUIRE(err.category == ErrorCategory::DEPRECATED_PROPERTY);
    REQUIRE(err.message.find("deprecated") != std::string::npos);
    REQUIRE(err.message.find("Use 'new_name' instead") != std::string::npos);
}

TEST_CASE("validate_all collects errors at multiple depths", "[validate][multi-error][nested]") {
    // Nested schema
    Dictionary schema = parse_json(R"({
        "type": "object",
        "required": ["config"],
        "properties": {
            "config": {
                "type": "object",
                "required": ["host", "port"],
                "properties": {
                    "host": {"type": "string"},
                    "port": {"type": "integer"}
                }
            }
        }
    })");

    // Data with partial nesting
    Dictionary data = parse_json(R"({
        "config": {
            "port": 8080
        }
    })");

    auto result = validate_all(data, schema);

    REQUIRE_FALSE(result.is_valid());
    // Should have error for missing 'host' in nested config
    REQUIRE(result.errors.size() >= 1);

    bool found_host_error = false;
    for (const auto& err : result.errors) {
        if (err.message.find("missing required key 'config.host'") != std::string::npos) {
            found_host_error = true;
            REQUIRE(err.depth == 1);  // Nested one level deep
        }
    }
    REQUIRE(found_host_error);
}

TEST_CASE("validate_all format produces readable output", "[validate][multi-error][format]") {
    Dictionary schema = parse_json(R"({
        "type": "object",
        "required": ["name", "port"],
        "properties": {
            "name": {"type": "string"},
            "port": {"type": "integer"},
            "debug": {
                "type": "boolean",
                "deprecated": true,
                "description": "Use 'verbose' instead"
            }
        }
    })");

    Dictionary data = parse_json(R"({
        "debug": true
    })");

    auto result = validate_all(data, schema);
    std::string formatted = result.format();

    REQUIRE(formatted.find("Validation failed") != std::string::npos);
    REQUIRE(formatted.find("missing required key 'name'") != std::string::npos);
    REQUIRE(formatted.find("missing required key 'port'") != std::string::npos);
    REQUIRE(formatted.find("deprecated") != std::string::npos);
    REQUIRE(formatted.find("2 errors") != std::string::npos);
    REQUIRE(formatted.find("1 deprecation") != std::string::npos);
}

TEST_CASE("validate backward compatibility returns first error", "[validate][backward-compat]") {
    Dictionary schema = parse_json(R"({
        "type": "object",
        "required": ["name", "port"],
        "properties": {
            "name": {"type": "string"},
            "port": {"type": "integer"}
        }
    })");

    Dictionary data = parse_json(R"({})");

    // Old API should still work and return an error
    auto error = validate(data, schema);
    REQUIRE(error.has_value());
    REQUIRE(error->find("missing required") != std::string::npos);
}

TEST_CASE("validate_all collects array item errors", "[validate][multi-error][array]") {
    Dictionary schema = parse_json(R"({
        "type": "object",
        "properties": {
            "items": {
                "type": "array",
                "items": {
                    "type": "object",
                    "required": ["id"],
                    "properties": {
                        "id": {"type": "integer"}
                    }
                }
            }
        }
    })");

    Dictionary data = parse_json(R"({
        "items": [
            {},
            {"id": 2},
            {}
        ]
    })");

    auto result = validate_all(data, schema);

    REQUIRE_FALSE(result.is_valid());
    // Should find missing 'id' in items[0] and items[2]
    // Note: might also catch an error from validate_node for the whole array
    REQUIRE(result.errors.size() >= 2);

    bool found_item_0 = false;
    bool found_item_2 = false;
    for (const auto& err : result.errors) {
        if (err.message.find("items[0].id") != std::string::npos ||
            err.message.find("items/0/id") != std::string::npos) {
            found_item_0 = true;
        }
        if (err.message.find("items[2].id") != std::string::npos ||
            err.message.find("items/2/id") != std::string::npos) {
            found_item_2 = true;
        }
    }
    REQUIRE(found_item_0);
    REQUIRE(found_item_2);
}
