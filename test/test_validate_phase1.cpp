#include <catch2/catch_all.hpp>
#include "ps/parsec.hpp"
#include "ps/validate.hpp"

using namespace ps;

TEST_CASE("validate phase1: basic types and numeric bounds", "[validate][phase1]") {
    Dictionary schema;
    Dictionary a; a["type"] = Value("integer"); a["required"] = Value(true); a["minimum"] = Value(int64_t(0)); a["maximum"] = Value(int64_t(10));
    Dictionary b; b["type"] = Value("number"); b["minimum"] = Value(0.5);
    Dictionary c; c["type"] = Value("string"); c["minLength"] = Value(int64_t(2)); c["maxLength"] = Value(int64_t(5));
    schema["a"] = Value(a);
    schema["b"] = Value(b);
    schema["c"] = Value(c);

    SECTION("valid values") {
        Dictionary cfg; cfg["a"] = Value(int64_t(3)); cfg["b"] = Value(1.5); cfg["c"] = Value(std::string("hey"));
        auto e = validate(cfg, schema);
        REQUIRE(!e.has_value());
    }

    SECTION("missing required") {
        Dictionary cfg; cfg["b"] = Value(1.0);
        auto e = validate(cfg, schema);
        REQUIRE(e.has_value());
        REQUIRE(e.value().find("missing required") != std::string::npos);
    }

    SECTION("numeric out of range") {
        Dictionary cfg; cfg["a"] = Value(int64_t(11)); cfg["b"] = Value(0.25); cfg["c"] = Value(std::string("ok"));
        auto e = validate(cfg, schema);
        REQUIRE(e.has_value());
    }

    SECTION("string length violations") {
        Dictionary cfg; cfg["a"] = Value(int64_t(3)); cfg["b"] = Value(2.0);
        cfg["c"] = Value(std::string("x"));
        auto e = validate(cfg, schema);
        REQUIRE(e.has_value());
    }
}

TEST_CASE("validate phase1: enum and additionalProperties", "[validate][phase1]") {
    Dictionary schema;
    Dictionary top; // use full-schema with type/object/properties
    top["type"] = Value("object");
    Dictionary props;
    Dictionary color; color["type"] = Value("string"); color["enum"] = Value(Value::list_t{ Value(std::string("red")), Value(std::string("green")), Value(std::string("blue")) });
    props["color"] = Value(color);
    top["properties"] = Value(props);
    top["additionalProperties"] = Value(false);

    auto err_ok = validate(Dictionary(), top); // top-level empty data is fine
    (void)err_ok;

    SECTION("enum allowed") {
        Dictionary cfg; cfg["color"] = Value(std::string("green"));
        auto e = validate(cfg, top);
        REQUIRE(!e.has_value());
    }

    SECTION("enum disallowed") {
        Dictionary cfg; cfg["color"] = Value(std::string("yellow"));
        auto e = validate(cfg, top);
        REQUIRE(e.has_value());
    }

    SECTION("additionalProperties blocked") {
        Dictionary cfg; cfg["color"] = Value(std::string("blue")); cfg["extra"] = Value(42);
        auto e = validate(cfg, top);
        REQUIRE(e.has_value());
        REQUIRE(e.value().find("not allowed") != std::string::npos);
    }
}

TEST_CASE("validate phase1: arrays, items, minItems/maxItems", "[validate][phase1]") {
    Dictionary schema;
    Dictionary top; top["type"] = Value("object");
    Dictionary props; 
    Dictionary arrs; arrs["type"] = Value("array");
    Dictionary itemSchema; itemSchema["type"] = Value("integer");
    arrs["items"] = Value(itemSchema);
    arrs["minItems"] = Value(int64_t(1)); arrs["maxItems"] = Value(int64_t(3));
    props["nums"] = Value(arrs);
    top["properties"] = Value(props);

    SECTION("array valid sizes") {
        Dictionary cfg; Value::list_t L; L.emplace_back(int64_t(1)); L.emplace_back(int64_t(2)); cfg["nums"] = Value(L);
        auto e = validate(cfg, top);
        REQUIRE(!e.has_value());
    }

    SECTION("array too few") {
        Dictionary cfg; Value::list_t L; cfg["nums"] = Value(L);
        auto e = validate(cfg, top);
        REQUIRE(e.has_value());
    }

    SECTION("array element wrong type") {
        Dictionary cfg; Value::list_t L; L.emplace_back(std::string("x")); cfg["nums"] = Value(L);
        auto e = validate(cfg, top);
        REQUIRE(e.has_value());
    }
}

TEST_CASE("validate phase1: $ref resolution (local)", "[validate][phase1]") {
    // Build a schema that uses local definitions and $ref
    Dictionary schema;
    schema["definitions"] = Value(Dictionary{{"PositiveInt", Value(Dictionary{{"type", Value(std::string("integer"))},{"minimum", Value(int64_t(1))}})}});
    Dictionary top; top["type"] = Value("object");
    Dictionary props; Dictionary p; p["$ref"] = Value(std::string("#/definitions/PositiveInt")); props["count"] = Value(p); top["properties"] = Value(props);
    schema["root"] = Value(top);

    // Validate correct
    Dictionary data; data["count"] = Value(int64_t(5));
    // validate against the sub-schema: we expect validate(data, schema_root_that_contains_definitions_and_root)
    Dictionary full; full = schema; // contains definitions and 'root'
    // Note: our simple resolve_local_ref expects refs relative to the schema passed in; pass full.root (root is top-level)
    // Place the actual schema at top-level for convenience
    full = top; // but keep definitions at top-level is necessary; attach definitions into the same dict
    full["definitions"] = schema["definitions"];

    auto e = validate(data, full);
    REQUIRE(!e.has_value());

    // invalid when below minimum
    Dictionary bad; bad["count"] = Value(int64_t(0));
    auto e2 = validate(bad, full);
    REQUIRE(e2.has_value());
}
