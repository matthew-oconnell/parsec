#include <catch2/catch_all.hpp>
#include "ps/parsec.hpp"
#include "ps/validate.hpp"

using namespace ps;

// Original validate tests
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

// Phase1 tests
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

// Phase2 tests
TEST_CASE("validate phase2: oneOf/anyOf/allOf", "[validate][phase2]") {
    Dictionary schema;
    // allOf: must be both integer and >= 5
    Dictionary s1; s1["type"] = Value(std::string("integer"));
    Dictionary s2; s2["minimum"] = Value(int64_t(5));
    Dictionary all; all["allOf"] = Value(Value::list_t{ Value(s1), Value(s2) });

    SECTION("allOf success") {
        Dictionary d; d.scalar = Value(int64_t(6));
        auto e = validate(d, all);
        REQUIRE(!e.has_value());
    }

    SECTION("allOf failure") {
        Dictionary d; d.scalar = Value(int64_t(3));
        auto e = validate(d, all);
        REQUIRE(e.has_value());
    }

    // anyOf: either integer or string
    Dictionary any; any["anyOf"] = Value(Value::list_t{ Value(s1), Value(Dictionary{{"type", Value(std::string("string"))}}) });
    SECTION("anyOf match") { Dictionary d; d.scalar = Value(std::string("hi")); REQUIRE(!validate(d, any).has_value()); }
    SECTION("anyOf no match") { Dictionary d; d.scalar = Value(1.23); REQUIRE(validate(d, any).has_value()); }

    // oneOf: exactly one alternative
    Dictionary o1; o1["type"] = Value(std::string("integer")); o1["minimum"] = Value(int64_t(0));
    Dictionary o2; o2["type"] = Value(std::string("integer")); o2["maximum"] = Value(int64_t(10));
    Dictionary one; one["oneOf"] = Value(Value::list_t{ Value(o1), Value(o2) });
    SECTION("oneOf ambiguous") { Dictionary d; d.scalar = Value(int64_t(5)); REQUIRE(validate(d, one).has_value()); }
}

TEST_CASE("validate phase2: tuple items and additionalItems", "[validate][phase2]") {
    Dictionary schema; schema["type"] = Value(std::string("array"));
    // tuple: [int, string], additionalItems=false
    Value::list_t tupleSchemas; tupleSchemas.emplace_back(Dictionary{{"type", Value(std::string("integer"))}});
    tupleSchemas.emplace_back(Dictionary{{"type", Value(std::string("string"))}});
    schema["items"] = Value(tupleSchemas);
    schema["additionalItems"] = Value(false);

    SECTION("tuple valid") { Value::list_t L; L.emplace_back(int64_t(1)); L.emplace_back(std::string("ok")); Dictionary d; d.scalar = Value(L); auto e = validate(d, schema); REQUIRE(!e.has_value()); }
    SECTION("tuple extra item") { Value::list_t L; L.emplace_back(int64_t(1)); L.emplace_back(std::string("ok")); L.emplace_back(int64_t(3)); Dictionary d; d.scalar = Value(L); auto e = validate(d, schema); REQUIRE(e.has_value()); }
}

TEST_CASE("validate phase2: uniqueItems and patternProperties", "[validate][phase2]") {
    Dictionary obj; obj["type"] = Value(std::string("object"));
    Dictionary props; props["id"] = Value(Dictionary{{"type", Value(std::string("integer"))}});
    obj["properties"] = Value(props);
    // patternProperties: keys starting with foo- must be strings
    Dictionary pat; pat["^foo-.*"] = Value(Dictionary{{"type", Value(std::string("string"))}});
    obj["patternProperties"] = Value(pat);

    SECTION("patternProperties match") {
        Dictionary cfg; cfg["foo-bar"] = Value(std::string("yes")); cfg["id"] = Value(int64_t(1)); auto e = validate(cfg, obj); REQUIRE(!e.has_value()); }

    SECTION("patternProperties mismatch") {
        Dictionary cfg; cfg["foo-baz"] = Value(int64_t(5)); auto e = validate(cfg, obj); REQUIRE(e.has_value()); }

    // uniqueItems
    Dictionary arr; arr["type"] = Value(std::string("array")); arr["uniqueItems"] = Value(true);
    Value::list_t L; L.emplace_back(int64_t(1)); L.emplace_back(int64_t(2)); L.emplace_back(int64_t(1)); arr["items"] = Value(Dictionary{{"type", Value(std::string("integer"))}});
    SECTION("uniqueItems violation") { Dictionary d; d.scalar = Value(L); auto e = validate(d, arr); REQUIRE(e.has_value()); }
}



// Additional-properties tests merged
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
        addSchema["required"] = Value(Value::list_t{ Value(std::string("x")) });
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
        Dictionary patterns; patterns["^pref_.*"] = Value(Dictionary{{"type", Value(std::string("integer"))}});
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
