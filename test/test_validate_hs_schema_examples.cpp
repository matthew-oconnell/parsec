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
}
