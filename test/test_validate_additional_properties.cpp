#include <catch2/catch_all.hpp>
#include "ps/parsec.hpp"
#include "ps/validate.hpp"

using namespace ps;

TEST_CASE("validate additionalProperties as schema", "[validate][additionalProperties]") {
    Dictionary schema;
    schema["type"] = Value(std::string("object"));
    Dictionary props;
    props["id"] = Value(Dictionary{{"type", Value(std::string("integer"))}});
    schema["properties"] = Value(props);
    // additionalProperties is a schema that requires string
    Dictionary addSchema; addSchema["type"] = Value(std::string("string"));
    schema["additionalProperties"] = Value(addSchema);

    SECTION("extra property validated by schema passes") {
        Dictionary cfg;
        cfg["id"] = Value(int64_t(1));
        cfg["note"] = Value(std::string("ok"));
        auto e = validate(cfg, schema);
        REQUIRE(!e.has_value());
    }

    SECTION("extra property fails schema") {
        Dictionary cfg;
        cfg["id"] = Value(int64_t(1));
        cfg["note"] = Value(int64_t(5));
        auto e = validate(cfg, schema);
        REQUIRE(e.has_value());
    }
}

TEST_CASE("validate minProperties and maxProperties", "[validate][minmaxprops]") {
    Dictionary schema; schema["type"] = Value(std::string("object"));
    schema["minProperties"] = Value(int64_t(2));
    schema["maxProperties"] = Value(int64_t(3));

    SECTION("too few properties") {
        Dictionary cfg; cfg["a"] = Value(1);
        auto e = validate(cfg, schema);
        REQUIRE(e.has_value());
    }
    SECTION("within bounds") {
        Dictionary cfg; cfg["a"] = Value(1); cfg["b"] = Value(2);
        auto e = validate(cfg, schema);
        REQUIRE(!e.has_value());
    }
    SECTION("too many properties") {
        Dictionary cfg; cfg["a"] = Value(1); cfg["b"] = Value(2); cfg["c"] = Value(3); cfg["d"] = Value(4);
        auto e = validate(cfg, schema);
        REQUIRE(e.has_value());
    }
}

TEST_CASE("additionalProperties schema can be an object schema with required fields", "[validate][additionalProperties][object-schema]") {
    Dictionary schema; schema["type"] = Value(std::string("object"));
    // no explicit properties
    Dictionary addSchema; addSchema["type"] = Value(std::string("object"));
    // additional properties must be objects that contain key 'x'
    addSchema["required"] = Value(Dictionary{{"0", Value(std::string("x"))}});
    schema["additionalProperties"] = Value(addSchema);

    SECTION("extra property missing required field fails") {
        Dictionary cfg; cfg["extra"] = Value(Dictionary{{"y", Value(1)}});
        auto e = validate(cfg, schema);
        REQUIRE(e.has_value());
    }

    SECTION("extra property meeting object schema passes") {
        Dictionary cfg; cfg["extra"] = Value(Dictionary{{"x", Value(1)}});
        auto e = validate(cfg, schema);
        REQUIRE(!e.has_value());
    }
}

TEST_CASE("patternProperties take precedence over additionalProperties", "[validate][patternProperties][additionalProperties]") {
    Dictionary schema; schema["type"] = Value(std::string("object"));
    // patternProperties: keys starting with pref_ must be integers
    Dictionary patterns; patterns["^pref_"] = Value(Dictionary{{"type", Value(std::string("integer"))}});
    schema["patternProperties"] = Value(patterns);
    // additionalProperties requires strings
    Dictionary addSchema; addSchema["type"] = Value(std::string("string"));
    schema["additionalProperties"] = Value(addSchema);

    SECTION("patternProperties match validated by pattern schema") {
        Dictionary cfg; cfg["pref_a"] = Value(int64_t(5));
        auto e = validate(cfg, schema);
        REQUIRE(!e.has_value());
    }

    SECTION("patternProperties mismatch fails even though additionalProperties would accept") {
        Dictionary cfg; cfg["pref_a"] = Value(std::string("not-int"));
        // patternProperties expects integer; additionalProperties (string) should not be applied
        auto e = validate(cfg, schema);
        REQUIRE(e.has_value());
    }

    SECTION("non-matching properties fall through to additionalProperties") {
        Dictionary cfg; cfg["other"] = Value(std::string("ok"));
        auto e = validate(cfg, schema);
        REQUIRE(!e.has_value());
    }
}
