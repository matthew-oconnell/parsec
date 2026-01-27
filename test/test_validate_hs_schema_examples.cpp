#include <catch2/catch_all.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "ps/parse.h"
#include "ps/validate.h"

namespace fs = std::filesystem;

static std::string slurp_file(const fs::path& path) {
    std::ifstream in(path);
    REQUIRE(in.good());
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

TEST_CASE("hs_schema examples: not-anyof-prefix valid.json validates", "[validate][json-schema]") {
#ifndef EXAMPLES_DIR
    FAIL("EXAMPLES_DIR not defined");
#endif

    fs::path examples = fs::path(EXAMPLES_DIR);
    REQUIRE(fs::exists(examples));

    fs::path schema_path = examples / "not-anyof-prefix" / "hs_schema.json";
    fs::path data_path = examples / "not-anyof-prefix" / "valid.json";

    REQUIRE(fs::exists(schema_path));
    REQUIRE(fs::exists(data_path));

    const std::string schema_content = slurp_file(schema_path);
    const std::string data_content = slurp_file(data_path);

    const ps::Dictionary schema = ps::parse(schema_content);
    ps::Dictionary data = ps::parse(data_content);

    // Mirror CLI behavior: defaults may be applied before validation.
    data = ps::setDefaults(data, schema);

    const ps::ValidationResult result = ps::validate_all(data, schema, data_content);
    REQUIRE(result.is_valid());
}

TEST_CASE("hs_schema examples: not-anyof-prefix bad.json rejected", "[validate][json-schema][not][anyOf]") {
#ifndef EXAMPLES_DIR
    FAIL("EXAMPLES_DIR not defined");
#endif

    fs::path examples = fs::path(EXAMPLES_DIR);
    REQUIRE(fs::exists(examples));

    fs::path schema_path = examples / "not-anyof-prefix" / "hs_schema.json";
    fs::path data_path = examples / "not-anyof-prefix" / "bad.json";

    REQUIRE(fs::exists(schema_path));
    REQUIRE(fs::exists(data_path));

    const std::string schema_content = slurp_file(schema_path);
    const std::string data_content = slurp_file(data_path);

    const ps::Dictionary schema = ps::parse(schema_content);
    ps::Dictionary data = ps::parse(data_content);

    // Mirror CLI behavior: defaults may be applied before validation.
    data = ps::setDefaults(data, schema);

    const ps::ValidationResult result = ps::validate_all(data, schema, data_content);
    
    // bad.json has "mesh adaptation" inside sequence[0], which should be rejected
    // by the schema's "not" + "anyOf" constraint on sequence items
    REQUIRE_FALSE(result.is_valid());
    REQUIRE(result.error_count() > 0);
    
    // Error should mention the forbidden field in the sequence
    const std::string formatted = result.format();
    INFO("Validation errors:\n" << formatted);
    REQUIRE(formatted.find("sequence") != std::string::npos);
    REQUIRE(formatted.find("mesh adaptation") != std::string::npos);
    REQUIRE(formatted.find("not allowed") != std::string::npos);
    
    // Verify the error points to the correct line (line 5 where "mesh adaptation" is in sequence[0])
    // not line 14 where "mesh adaptation" is at root
    REQUIRE(formatted.find("line 5") != std::string::npos);
}

TEST_CASE("not keyword: simple case rejects matching data", "[validate][json-schema][not]") {
    ps::Dictionary schema;
    schema["type"] = "object";
    
    // Create a 'not' schema that matches objects with a 'forbidden' property
    ps::Dictionary not_schema;
    not_schema["type"] = "object";
    not_schema["required"][0] = "forbidden";
    schema["not"] = not_schema;
    
    ps::Dictionary valid;
    valid["allowed"] = "value";
    
    ps::Dictionary invalid;
    invalid["forbidden"] = "bad";
    
    auto valid_result = ps::validate(valid, schema);
    REQUIRE_FALSE(valid_result.has_value());
    
    auto invalid_result = ps::validate(invalid, schema);
    REQUIRE(invalid_result.has_value());
    REQUIRE(invalid_result->find("must not validate") != std::string::npos);
}

TEST_CASE("not keyword with anyOf: detects forbidden properties", "[validate][json-schema][not]") {
    ps::Dictionary schema;
    schema["type"] = "object";
    
    // Forbid objects that have "prop1" OR "prop2" as required
    ps::Dictionary not_schema;
    ps::Dictionary alt1;
    alt1["required"][0] = "prop1";
    ps::Dictionary alt2;
    alt2["required"][0] = "prop2";
    not_schema["anyOf"][0] = alt1;
    not_schema["anyOf"][1] = alt2;
    schema["not"] = not_schema;
    
    ps::Dictionary valid;
    valid["prop3"] = "allowed";
    
    ps::Dictionary invalid1;
    invalid1["prop1"] = "forbidden";
    
    ps::Dictionary invalid2;
    invalid2["prop2"] = "also forbidden";
    
    REQUIRE_FALSE(ps::validate(valid, schema).has_value());
    
    auto err1 = ps::validate(invalid1, schema);
    REQUIRE(err1.has_value());
    REQUIRE(err1->find("prop1") != std::string::npos);
    REQUIRE(err1->find("not allowed") != std::string::npos);
    
    auto err2 = ps::validate(invalid2, schema);
    REQUIRE(err2.has_value());
    REQUIRE(err2->find("prop2") != std::string::npos);
    REQUIRE(err2->find("not allowed") != std::string::npos);
}

TEST_CASE("prefixItems: validates array items positionally", "[validate][json-schema][prefixItems]") {
    ps::Dictionary schema;
    schema["type"] = "array";
    
    // First item must be string, second must be integer
    ps::Dictionary item0;
    item0["type"] = "string";
    ps::Dictionary item1;
    item1["type"] = "integer";
    schema["prefixItems"][0] = item0;
    schema["prefixItems"][1] = item1;
    
    ps::Dictionary valid;
    valid[0] = "hello";
    valid[1] = 42;
    
    ps::Dictionary invalid;
    invalid[0] = 123;  // Should be string
    invalid[1] = "wrong";  // Should be integer
    
    REQUIRE_FALSE(ps::validate(valid, schema).has_value());
    
    auto err = ps::validate(invalid, schema);
    REQUIRE(err.has_value());
    REQUIRE(err->find("expected type") != std::string::npos);
}

TEST_CASE("prefixItems with items: validates prefix then remaining items", "[validate][json-schema][prefixItems]") {
    ps::Dictionary schema;
    schema["type"] = "array";
    
    // First two items are string, integer
    schema["prefixItems"][0]["type"] = "string";
    schema["prefixItems"][1]["type"] = "integer";
    
    // Remaining items must be boolean
    schema["items"]["type"] = "boolean";
    
    ps::Dictionary valid;
    valid[0] = "text";
    valid[1] = 99;
    valid[2] = true;
    valid[3] = false;
    
    ps::Dictionary invalid;
    invalid[0] = "text";
    invalid[1] = 99;
    invalid[2] = "not boolean";  // Should be boolean
    
    REQUIRE_FALSE(ps::validate(valid, schema).has_value());
    
    auto err = ps::validate(invalid, schema);
    REQUIRE(err.has_value());
    REQUIRE(err->find("expected type") != std::string::npos);
}
