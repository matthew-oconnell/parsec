#include <catch2/catch_all.hpp>
#include "ps/parsec.hpp"
#include "ps/validate.hpp"

using namespace ps;

TEST_CASE("validate: valid and invalid cases", "[validate]") {
    // Build schema as Dictionary
    Dictionary schema;
    // name: required string
    Dictionary nameSpec; nameSpec["type"] = Value("string"); nameSpec["required"] = Value(true);
    // port: required integer, between 1 and 65535
    Dictionary portSpec; portSpec["type"] = Value("integer"); portSpec["required"] = Value(true); portSpec["minimum"] = Value(int64_t(1)); portSpec["maximum"] = Value(int64_t(65535));
    // debug: optional boolean
    Dictionary debugSpec; debugSpec["type"] = Value("boolean"); debugSpec["required"] = Value(false);

    schema["name"] = Value(nameSpec);
    schema["port"] = Value(portSpec);
    schema["debug"] = Value(debugSpec);

    SECTION("valid config") {
        Dictionary cfg;
        cfg["name"] = Value(std::string("example"));
        cfg["port"] = Value(int64_t(8080));
        cfg["debug"] = Value(true);

        auto err = validate(cfg, schema);
        REQUIRE(!err.has_value());
    }

    SECTION("invalid: missing required") {
        Dictionary cfg;
        cfg["port"] = Value(int64_t(80));
        auto err = validate(cfg, schema);
        REQUIRE(err.has_value());
        REQUIRE(err.value().find("missing required") != std::string::npos);
    }

    SECTION("invalid: wrong type") {
        Dictionary cfg;
        cfg["name"] = Value(int64_t(123));
        cfg["port"] = Value(int64_t(80));
        auto err = validate(cfg, schema);
        REQUIRE(err.has_value());
        REQUIRE(err.value().find("expected type") != std::string::npos);
    }

    SECTION("invalid: out of range") {
        Dictionary cfg;
        cfg["name"] = Value(std::string("example"));
        cfg["port"] = Value(int64_t(70000));
        auto err = validate(cfg, schema);
        REQUIRE(err.has_value());
        REQUIRE(err.value().find("above maximum") != std::string::npos);
    }
}
