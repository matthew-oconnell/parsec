#include <catch2/catch_all.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include "ps/parsec.h"
#include "ps/validate.h"

namespace fs = std::filesystem;

using namespace ps;

// Original validate tests
TEST_CASE("validate: valid and invalid cases", "[validate]") {
    // Build schema as Dictionary
    Dictionary schema;
    // name: required string
    Dictionary nameSpec;
    nameSpec["type"] = "string";
    nameSpec["required"] = true;
    // port: required integer, between 1 and 65535
    Dictionary portSpec;
    portSpec["type"] = "integer";
    portSpec["required"] = true;
    portSpec["minimum"] = int64_t(1);
    portSpec["maximum"] = int64_t(65535);
    // debug: optional boolean
    Dictionary debugSpec;
    debugSpec["type"] = "boolean";
    debugSpec["required"] = false;

    schema["name"] = nameSpec;
    schema["port"] = portSpec;
    schema["debug"] = debugSpec;

    SECTION("valid config") {
        Dictionary cfg;
        cfg["name"] = std::string("example");
        cfg["port"] = int64_t(8080);
        cfg["debug"] = true;

        auto err = validate(cfg, schema);
        REQUIRE(!err.has_value());
    }

    SECTION("invalid: missing required") {
        Dictionary cfg;
        cfg["port"] = 80;
        auto err = validate(cfg, schema);
        REQUIRE(err.has_value());
        REQUIRE(err.value().find("missing required") != std::string::npos);
    }

    SECTION("invalid: wrong type") {
        Dictionary cfg;
        cfg["name"] = 123;
        cfg["port"] = 80;
        auto err = validate(cfg, schema);
        REQUIRE(err.has_value());
        REQUIRE(err.value().find("expected type") != std::string::npos);
    }

    SECTION("invalid: out of range") {
        Dictionary cfg;
        cfg["name"] = "example";
        cfg["port"] = 70000;
        auto err = validate(cfg, schema);
        REQUIRE(err.has_value());
        REQUIRE(err.value().find("above maximum") != std::string::npos);
    }
}

// Phase1 tests
TEST_CASE("validate phase1: basic types and numeric bounds", "[validate][phase1]") {
    Dictionary schema;
    Dictionary a;
    a["type"] = "integer";
    a["required"] = true;
    a["minimum"] = 0;
    a["maximum"] = 10;
    Dictionary b;
    b["type"] = "number";
    b["minimum"] = 0.5;
    Dictionary c;
    c["type"] = "string";
    c["minLength"] = 2;
    c["maxLength"] = 5;
    schema["a"] = a;
    schema["b"] = b;
    schema["c"] = c;

    SECTION("valid values") {
        Dictionary cfg;
        cfg["a"] = 3;
        cfg["b"] = 1.5;
        cfg["c"] = "hey";
        auto e = validate(cfg, schema);
        REQUIRE(!e.has_value());
    }

    SECTION("missing required") {
        Dictionary cfg;
        cfg["b"] = 1.0;
        auto e = validate(cfg, schema);
        REQUIRE(e.has_value());
        REQUIRE(e.value().find("missing required") != std::string::npos);
    }

    SECTION("numeric out of range") {
        Dictionary cfg;
        cfg["a"] = 11;
        cfg["b"] = 0.25;
        cfg["c"] = "ok";
        auto e = validate(cfg, schema);
        REQUIRE(e.has_value());
    }

    SECTION("string length violations") {
        Dictionary cfg;
        cfg["a"] = 3;
        cfg["b"] = 2.0;
        cfg["c"] = "x";
        auto e = validate(cfg, schema);
        REQUIRE(e.has_value());
    }
}

TEST_CASE("validate phase1: enum and additionalProperties", "[validate][phase1]") {
    Dictionary schema;
    Dictionary top;  // use full-schema with type/object/properties
    top["type"] = "object";
    Dictionary props;
    Dictionary color;
    color["type"] = "string";
    color["enum"] = std::vector<std::string>{"red", "green", "blue"};
    props["color"] = color;
    top["properties"] = props;
    top["additionalProperties"] = false;

    auto err_ok = validate(Dictionary(), top);  // top-level empty data is fine
    (void)err_ok;

    SECTION("enum allowed") {
        Dictionary cfg;
        cfg["color"] = "green";
        auto e = validate(cfg, top);
        REQUIRE(!e.has_value());
    }

    SECTION("enum disallowed") {
        Dictionary cfg;
        cfg["color"] = "yellow";
        auto e = validate(cfg, top);
        REQUIRE(e.has_value());
    }

    SECTION("additionalProperties blocked") {
        Dictionary cfg;
        cfg["color"] = "blue";
        cfg["extra"] = 42;
        auto e = validate(cfg, top);
        REQUIRE(e.has_value());
        REQUIRE(e.value().find("not valid") != std::string::npos);
    }
}

TEST_CASE("validate phase1: arrays, items, minItems/maxItems", "[validate][phase1]") {
    Dictionary schema;
    Dictionary top;
    top["type"] = "object";
    Dictionary props;
    Dictionary arrs;
    arrs["type"] = "array";
    Dictionary itemSchema;
    itemSchema["type"] = "integer";
    arrs["items"] = itemSchema;
    arrs["minItems"] = int64_t(1);
    arrs["maxItems"] = int64_t(3);
    props["nums"] = arrs;
    top["properties"] = props;

    SECTION("array valid sizes") {
        Dictionary cfg;
        std::vector<int> L{1, 2};
        cfg["nums"] = L;
        auto e = validate(cfg, top);
        REQUIRE(!e.has_value());
    }

    SECTION("array too few") {
        Dictionary cfg;
        std::vector<Dictionary> L;
        cfg["nums"] = L;
        auto e = validate(cfg, top);
        REQUIRE(e.has_value());
    }

    SECTION("array element wrong type") {
        Dictionary cfg;
        std::vector<std::string> L;
        L.emplace_back("x");
        cfg["nums"] = L;
        auto e = validate(cfg, top);
        REQUIRE(e.has_value());
    }
}

TEST_CASE("validate phase1: $ref resolution (local)", "[validate][phase1]") {
    // Build a schema that uses local definitions and $ref
    Dictionary schema;
    schema["definitions"] = Dictionary{{"PositiveInt", Dictionary{{"type", "integer"}, {"minimum", int64_t(1)}}}};
    Dictionary top;
    top["type"] = "object";
    Dictionary props;
    Dictionary p;
    p["$ref"] = "#/definitions/PositiveInt";
    props["count"] = p;
    top["properties"] = props;
    schema["root"] = top;

    // Validate correct
    Dictionary data;
    data["count"] = int64_t(5);
    // validate against the sub-schema: we expect validate(data,
    // schema_root_that_contains_definitions_and_root)
    Dictionary full;
    full = schema;  // contains definitions and 'root'
    // Note: our simple resolve_local_ref expects refs relative to the schema passed in; pass
    // full.root (root is top-level) Place the actual schema at top-level for convenience
    full = top;  // but keep definitions at top-level is necessary; attach definitions into the same
                 // dict
    full["definitions"] = schema["definitions"];

    auto e = validate(data, full);
    REQUIRE(!e.has_value());

    // invalid when below minimum
    Dictionary bad;
    bad["count"] = int64_t(0);
    auto e2 = validate(bad, full);
    REQUIRE(e2.has_value());
}

// Phase2 tests
TEST_CASE("validate phase2: oneOf/anyOf/allOf", "[validate][phase2]") {
    Dictionary schema;
    // allOf: must be both integer and >= 5
    Dictionary s1;
    s1["type"] = "integer";
    Dictionary s2;
    s2["minimum"] = int64_t(5);
    Dictionary all;
    all["allOf"] = std::vector<Dictionary>{s1, s2};
    SECTION("allOf success") {
        Dictionary d;
        d = 6;
        auto e = validate(d, all);
        REQUIRE(!e.has_value());
    }

    SECTION("allOf failure") {
        Dictionary d;
        d = 3;
        auto e = validate(d, all);
        REQUIRE(e.has_value());
    }

    // anyOf: either integer or string
    Dictionary any;
    any["anyOf"] = std::vector<Dictionary>{s1, Dictionary{{"type", "string"}}};
    SECTION("anyOf match") {
        Dictionary d;
        d = std::string("hi");
        REQUIRE(!validate(d, any).has_value());
    }
    SECTION("anyOf no match") {
        Dictionary d;
        d = 1.23;
        REQUIRE(validate(d, any).has_value());
    }

    // oneOf: exactly one alternative
    Dictionary o1;
    o1["type"] = "integer";
    o1["minimum"] = int64_t(0);
    Dictionary o2;
    o2["type"] = "integer";
    o2["maximum"] = int64_t(10);
    Dictionary one;
    one["oneOf"] = std::vector<Dictionary>{o1, o2};
    SECTION("oneOf ambiguous") {
        Dictionary d;
        d = int64_t(5);
        REQUIRE(validate(d, one).has_value());
    }
}

TEST_CASE("validate phase2: tuple items and additionalItems", "[validate][phase2]") {
    Dictionary schema;
    schema["type"] = "array";
    // tuple: [int, string], additionalItems=false
    std::vector<Dictionary> tupleSchemas;
    tupleSchemas.emplace_back(Dictionary{{"type", "integer"}});
    tupleSchemas.emplace_back(Dictionary{{"type", "string"}});
    schema["items"] = tupleSchemas;
    schema["additionalItems"] = false;

    SECTION("tuple valid") {
        std::vector<Dictionary> L;
        L.emplace_back(int64_t(1));
        L.emplace_back(std::string("ok"));
        Dictionary d;
        d = L;
        auto e = validate(d, schema);
        REQUIRE(!e.has_value());
    }
    SECTION("tuple extra item") {
        std::vector<Dictionary> L;
        L.emplace_back(int64_t(1));
        L.emplace_back(std::string("ok"));
        L.emplace_back(int64_t(3));
        Dictionary d;
        d = L;
        auto e = validate(d, schema);
        REQUIRE(e.has_value());
    }
}

TEST_CASE("validate phase2: uniqueItems and patternProperties", "[validate][phase2]") {
    Dictionary obj;
    obj["type"] = "object";
    Dictionary props;
    props["id"] = Dictionary{{"type", "integer"}};
    obj["properties"] = props;
    // patternProperties: keys starting with foo- must be strings
    Dictionary pat;
    pat["^foo-.*"] = Dictionary{{"type", "string"}};
    obj["patternProperties"] = pat;
    SECTION("patternProperties match") {
        Dictionary cfg;
        cfg["foo-bar"] = std::string("yes");
        cfg["id"] = int64_t(1);
        auto e = validate(cfg, obj);
        REQUIRE(!e.has_value());
    }

    SECTION("patternProperties mismatch") {
        Dictionary cfg;
        cfg["foo-baz"] = int64_t(5);
        auto e = validate(cfg, obj);
        REQUIRE(e.has_value());
    }

    // uniqueItems
    Dictionary arr;
    arr["type"] = "array";
    arr["uniqueItems"] = true;
    std::vector<Dictionary> L;
    L.emplace_back(int64_t(1));
    L.emplace_back(int64_t(2));
    L.emplace_back(int64_t(1));
    arr["items"] = Dictionary{{"type", "integer"}};
    SECTION("uniqueItems violation") {
        Dictionary d;
        d = L;
        auto e = validate(d, arr);
        REQUIRE(e.has_value());
    }
}

// Additional-properties tests merged
TEST_CASE("validate additionalProperties as schema", "[validate][additionalProperties]") {
    Dictionary schema;
    schema["type"] = "object";
    Dictionary props;
    props["id"] = Dictionary{{"type", "integer"}};
    schema["properties"] = props;
    // additionalProperties is a schema that requires string
    Dictionary addSchema;
    addSchema["type"] = "string";
    schema["additionalProperties"] = addSchema;
    SECTION("extra property validated by schema passes") {
        Dictionary cfg;
        cfg["id"] = int64_t(1);
        cfg["note"] = std::string("ok");
        auto e = validate(cfg, schema);
        REQUIRE(!e.has_value());
    }

    SECTION("extra property fails schema") {
        Dictionary cfg;
        cfg["id"] = int64_t(1);
        cfg["note"] = int64_t(5);
        auto e = validate(cfg, schema);
        REQUIRE(e.has_value());
    }
}

TEST_CASE("validate minProperties and maxProperties", "[validate][minmaxprops]") {
    Dictionary schema;
    schema["type"] = "object";
    schema["minProperties"] = int64_t(2);
    schema["maxProperties"] = int64_t(3);

    SECTION("too few properties") {
        Dictionary cfg;
        cfg["a"] = int64_t(1);
        auto e = validate(cfg, schema);
        REQUIRE(e.has_value());
    }
    SECTION("within bounds") {
        Dictionary cfg;
        cfg["a"] = int64_t(1);
        cfg["b"] = int64_t(2);
        auto e = validate(cfg, schema);
        REQUIRE(!e.has_value());
    }
    SECTION("too many properties") {
        Dictionary cfg;
        cfg["a"] = int64_t(1);
        cfg["b"] = int64_t(2);
        cfg["c"] = int64_t(3);
        cfg["d"] = int64_t(4);
        auto e = validate(cfg, schema);
        REQUIRE(e.has_value());
    }
}

TEST_CASE("additionalProperties schema can be an object schema with required fields",
          "[validate][additionalProperties][object-schema]") {
    Dictionary schema;
    schema["type"] = "object";
    // no explicit properties
    Dictionary addSchema;
    addSchema["type"] = "object";
    // additional properties must be objects that contain key 'x'
    addSchema["required"] = std::vector<Dictionary>{Dictionary("x")};
    schema["additionalProperties"] = addSchema;

    SECTION("extra property missing required field fails") {
        Dictionary cfg;
        cfg["extra"] = Dictionary{{"y", int64_t(1)}};
        auto e = validate(cfg, schema);
        REQUIRE(e.has_value());
    }

    SECTION("extra property meeting object schema passes") {
        Dictionary cfg;
        cfg["extra"] = Dictionary{{"x", int64_t(1)}};
        auto e = validate(cfg, schema);
        REQUIRE(!e.has_value());
    }
}

TEST_CASE("patternProperties take precedence over additionalProperties",
          "[validate][patternProperties][additionalProperties]") {
    Dictionary schema;
    schema["type"] = "object";
    // patternProperties: keys starting with pref_ must be integers
    Dictionary patterns;
    patterns["^pref_.*"] = Dictionary{{"type", "integer"}};
    schema["patternProperties"] = patterns;
    // additionalProperties requires strings
    Dictionary addSchema;
    addSchema["type"] = "string";
    schema["additionalProperties"] = addSchema;
    SECTION("patternProperties match validated by pattern schema") {
        Dictionary cfg;
        cfg["pref_a"] = int64_t(5);
        auto e = validate(cfg, schema);
        REQUIRE(!e.has_value());
    }

    SECTION("patternProperties mismatch fails even though additionalProperties would accept") {
        Dictionary cfg;
        cfg["pref_a"] = std::string("not-int");
        // patternProperties expects integer; additionalProperties (string) should not be applied
        auto e = validate(cfg, schema);
        REQUIRE(e.has_value());
    }

    SECTION("non-matching properties fall through to additionalProperties") {
        Dictionary cfg;
        cfg["other"] = std::string("ok");
        auto e = validate(cfg, schema);
        REQUIRE(!e.has_value());
    }
}



TEST_CASE("setDefaults: Default should not override existing value", "[defaults][bug]") {
    // Schema with a default value of 500 for "steps"
    Dictionary schema;
    schema["type"] = "object";
    Dictionary props;
    props["steps"] = Dictionary{{"type", "integer"}, {"default", int64_t(500)}};
    schema["properties"] = props;

    // User input with "steps" set to 100
    Dictionary input;
    input["steps"] = int64_t(100);
    
    // The expected behavior: setDefaults should preserve the user-provided value
    auto out = setDefaults(input, schema);
    
    // This should pass, but currently fails because setDefaults incorrectly 
    // applies the schema default (500) even though the user input has a value (100)
    REQUIRE(out.has("steps"));
    REQUIRE(out.at("steps").isInt());
    REQUIRE(out.at("steps").asInt() == 100); // Expected: 100, Actual: 500
    
    INFO("Schema default incorrectly overrode user-provided value");
    INFO("Expected steps=100 (user input), got steps=" << out.at("steps").asInt() << " (schema default)");
}

// Additional test case showing how this affects nested objects
TEST_CASE("setDefaults: Default should not override existing nested values", "[defaults][bug]") {
    // Schema with nested object that has defaults
    Dictionary schema;
    schema["type"] = "object";
    Dictionary props;
    
    // Create a nested schema with defaults
    Dictionary nestedSchema;
    nestedSchema["type"] = "object";
    Dictionary nestedProps;
    nestedProps["value"] = Dictionary{{"type", "integer"}, {"default", int64_t(500)}};
    nestedSchema["properties"] = nestedProps;
    
    props["nested"] = nestedSchema;
    schema["properties"] = props;

    // User input with a nested object that has a different value
    Dictionary input;
    Dictionary nestedInput;
    nestedInput["value"] = int64_t(100);
    input["nested"] = nestedInput;
    
    // The expected behavior: setDefaults should preserve the user-provided value in nested objects
    auto out = setDefaults(input, schema);
    
    REQUIRE(out.has("nested"));
    REQUIRE(out.at("nested").has("value"));
    REQUIRE(out.at("nested").at("value").isInt());
    REQUIRE(out.at("nested").at("value").asInt() == 100); // Expected: 100, Actual: might be 500
    
    INFO("Schema default incorrectly overrode user-provided nested value");
}

// Dictionary load_file(const std::filesystem::path& p) {
//     std::ifstream in(p);
//     REQUIRE(in.good());
//     std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
//     return ps::parse_json(content);
// }

// TEST_CASE("fix bug from upstream", "[validate]") {
//     // load schema from test/bug-assets/schema.json
//     fs::path schema_path = fs::path(EXAMPLES_DIR) / "bug-assets" / "schema.json";
//     auto schema = load_file(schema_path);

//     fs::path user_path = fs::path(EXAMPLES_DIR) / "bug-assets" / "user.json";
//     auto user = load_file(user_path);

//     auto merged = ps::setDefaults(user, schema);

//     REQUIRE(merged.at("HyperSolve").at("nonlinear solver settings").at("frechet derivative type").asString() ==
//             "finite-difference");
// }