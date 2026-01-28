#include <catch2/catch_all.hpp>
#include "ps/parsec.h"
#include "ps/validate.h"

using namespace ps;

TEST_CASE("setDefaults: basic property default", "[defaults]") {
    Dictionary schema;
    schema["type"] = "object";
    Dictionary props;
    props["port"] = Dictionary{{"type", "integer"}, {"default", int64_t(8080)}};
    schema["properties"] = props;

    Dictionary input;  // empty
    auto out = setDefaults(input, schema);
    REQUIRE(out.has("port"));
    REQUIRE(out.at("port").isInt());
    REQUIRE(out.at("port").asInt() == 8080);
}

TEST_CASE("setDefaults: nested object defaults", "[defaults]") {
    Dictionary innerSchema;
    innerSchema["type"] = "object";
    Dictionary innerProps;
    innerProps["x"] = Dictionary{{"type", "integer"}, {"default", int64_t(7)}};
    innerSchema["properties"] = innerProps;

    Dictionary schema;
    schema["type"] = "object";
    Dictionary props;
    props["child"] = innerSchema;
    schema["properties"] = props;

    Dictionary input;  // child missing
    auto out = setDefaults(input, schema);
    REQUIRE(out.has("child"));
    REQUIRE(out.at("child").has("x"));
    REQUIRE(out.at("child").at("x").asInt() == 7);
}

TEST_CASE("setDefaults: additionalProperties schema applied to existing extras", "[defaults]") {
    Dictionary schema;
    schema["type"] = "object";
    Dictionary add;
    add["type"] = "object";
    Dictionary addProps;
    addProps["y"] = Dictionary{{"type", "integer"}, {"default", int64_t(2)}};
    add["properties"] = addProps;
    schema["additionalProperties"] = add;
    Dictionary input;
    input["extra"] = Dictionary{{"z", int64_t(1)}};
    auto out = setDefaults(input, schema);
    REQUIRE(out.has("extra"));
    auto extra = out.at("extra");
    REQUIRE(extra.isDict());
    REQUIRE(extra.has("y"));
    REQUIRE(extra.at("y").asInt() == 2);
}

TEST_CASE("setDefaults: allOf composition applies defaults from all schemas", "[defaults]") {
    // Create a base schema with some properties
    Dictionary baseSchema;
    baseSchema["type"] = "object";
    Dictionary baseProps;
    baseProps["name"] = Dictionary{{"type", "string"}, {"default", "unknown"}};
    baseProps["count"] = Dictionary{{"type", "integer"}, {"default", int64_t(0)}};
    baseSchema["properties"] = baseProps;

    // Store base schema in definitions
    Dictionary definitions;
    definitions["BaseObject"] = baseSchema;

    // Create a schema using allOf
    Dictionary allOfItem;
    allOfItem["$ref"] = "#/definitions/BaseObject";

    std::vector<Dictionary> allOfArray;
    allOfArray.push_back(allOfItem);

    Dictionary composedSchema;
    composedSchema["allOf"] = allOfArray;

    // Create root schema with definitions
    Dictionary rootSchema;
    rootSchema["type"] = "object";
    rootSchema["definitions"] = definitions;
    Dictionary rootProps;
    rootProps["config"] = composedSchema;
    rootSchema["properties"] = rootProps;

    // Input with config object but missing the properties that have defaults
    Dictionary input;
    input["config"] = Dictionary();

    auto out = setDefaults(input, rootSchema);
    REQUIRE(out.has("config"));
    REQUIRE(out.at("config").has("name"));
    REQUIRE(out.at("config").at("name").asString() == "unknown");
    REQUIRE(out.at("config").has("count"));
    REQUIRE(out.at("config").at("count").asInt() == 0);
}

TEST_CASE("setDefaults: allOf with nested $ref applies defaults correctly", "[defaults]") {
    // Simulates the HyperSolve -> HyperSolveRequired -> HyperSolveBase -> Discretization Settings
    // structure

    // Create the innermost schema (like Discretization Settings)
    Dictionary discSettings;
    discSettings["type"] = "object";
    Dictionary discProps;
    discProps["lock limiter"] = Dictionary{{"type", "integer"}, {"default", int64_t(-1)}};
    discProps["buffer limiter"] = Dictionary{{"type", "integer"}, {"default", int64_t(1)}};
    discSettings["properties"] = discProps;

    // Create a base schema that references discSettings (like HyperSolveBase)
    Dictionary baseSchema;
    baseSchema["type"] = "object";
    Dictionary baseProps;
    baseProps["discretization settings"] =
                Dictionary{{"$ref", "#/definitions/Discretization Settings"}};
    baseSchema["properties"] = baseProps;

    // Create a composed schema with allOf (like HyperSolveRequired)
    Dictionary allOfItem;
    allOfItem["$ref"] = "#/definitions/HyperSolveBase";

    std::vector<Dictionary> allOfArray;
    allOfArray.push_back(allOfItem);

    Dictionary composedSchema;
    composedSchema["allOf"] = allOfArray;

    // Create root schema with definitions
    Dictionary rootSchema;
    rootSchema["type"] = "object";
    Dictionary definitions;
    definitions["Discretization Settings"] = discSettings;
    definitions["HyperSolveBase"] = baseSchema;
    definitions["HyperSolveRequired"] = composedSchema;
    rootSchema["definitions"] = definitions;

    Dictionary rootProps;
    rootProps["HyperSolve"] = Dictionary{{"$ref", "#/definitions/HyperSolveRequired"}};
    rootSchema["properties"] = rootProps;

    // Input with HyperSolve.discretization settings present but missing lock limiter
    Dictionary input;
    Dictionary hyperSolve;
    Dictionary discSettingsInput;
    discSettingsInput["buffer limiter"] = int64_t(2);  // User provided this
    hyperSolve["discretization settings"] = discSettingsInput;
    input["HyperSolve"] = hyperSolve;

    auto out = setDefaults(input, rootSchema);
    REQUIRE(out.has("HyperSolve"));
    REQUIRE(out.at("HyperSolve").has("discretization settings"));
    auto disc = out.at("HyperSolve").at("discretization settings");

    // User-provided value should be preserved
    REQUIRE(disc.has("buffer limiter"));
    REQUIRE(disc.at("buffer limiter").asInt() == 2);

    // Default should be applied for missing property
    REQUIRE(disc.has("lock limiter"));
    REQUIRE(disc.at("lock limiter").asInt() == -1);
}

TEST_CASE("setDefaults: schema with root-level $ref", "[defaults]") {
    // Create a schema with definitions
    Dictionary definitions;
    
    Dictionary mainSchema;
    mainSchema["type"] = "object";
    Dictionary props;
    props["port"] = Dictionary{{"type", "integer"}, {"default", int64_t(8080)}};
    props["host"] = Dictionary{{"type", "string"}, {"default", "localhost"}};
    mainSchema["properties"] = props;
    
    definitions["MainSettings"] = mainSchema;
    
    // Root schema has only $ref
    Dictionary rootSchema;
    rootSchema["$ref"] = "#/definitions/MainSettings";
    rootSchema["definitions"] = definitions;
    
    // Test with empty input
    Dictionary input;
    auto out = setDefaults(input, rootSchema);
    
    REQUIRE(out.has("port"));
    REQUIRE(out.at("port").asInt() == 8080);
    REQUIRE(out.has("host"));
    REQUIRE(out.at("host").asString() == "localhost");
}

TEST_CASE("setDefaults: schema with root-level $ref preserves user values", "[defaults]") {
    // Create a schema with definitions
    Dictionary definitions;
    
    Dictionary mainSchema;
    mainSchema["type"] = "object";
    Dictionary props;
    props["port"] = Dictionary{{"type", "integer"}, {"default", int64_t(8080)}};
    props["host"] = Dictionary{{"type", "string"}, {"default", "localhost"}};
    mainSchema["properties"] = props;
    
    definitions["MainSettings"] = mainSchema;
    
    // Root schema has only $ref
    Dictionary rootSchema;
    rootSchema["$ref"] = "#/definitions/MainSettings";
    rootSchema["definitions"] = definitions;
    
    // Test with partial user input
    Dictionary input;
    input["port"] = int64_t(3000);  // User provided value
    auto out = setDefaults(input, rootSchema);
    
    REQUIRE(out.has("port"));
    REQUIRE(out.at("port").asInt() == 3000);  // User value preserved
    REQUIRE(out.has("host"));
    REQUIRE(out.at("host").asString() == "localhost");  // Default applied
}

TEST_CASE("setDefaults: schema without explicit type but with properties infers object", "[defaults]") {
    // Schema has "properties" but no "type": "object" declaration
    // Should still apply defaults because properties implies object type
    Dictionary schema;
    // Deliberately NO "type" field here
    Dictionary props;
    props["port"] = Dictionary{{"type", "integer"}, {"default", int64_t(8080)}};
    props["host"] = Dictionary{{"type", "string"}, {"default", "localhost"}};
    schema["properties"] = props;

    Dictionary input;  // empty
    auto out = setDefaults(input, schema);
    
    REQUIRE(out.has("port"));
    REQUIRE(out.at("port").asInt() == 8080);
    REQUIRE(out.has("host"));
    REQUIRE(out.at("host").asString() == "localhost");
}
