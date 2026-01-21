#include <catch2/catch_test_macros.hpp>
#include "ps/parsec.h"
#include "ps/validate.h"

using namespace ps;

TEST_CASE("deprecated property is reported during validation", "[validate][deprecated]") {
    // Schema with a deprecated property
    Dictionary schema;
    schema["type"] = "object";

    Dictionary props;

    // Current property
    Dictionary portProp;
    portProp["type"] = "integer";
    portProp["description"] = "The server port number";
    props["port"] = portProp;

    // Deprecated property
    Dictionary serverPortProp;
    serverPortProp["type"] = "integer";
    serverPortProp["deprecated"] = true;
    serverPortProp["description"] =
                "DEPRECATED: Use 'port' instead. This property will be removed in v2.0.";
    props["server_port"] = serverPortProp;

    schema["properties"] = props;
    schema["additionalProperties"] = false;

    SECTION("using current property passes validation") {
        Dictionary data;
        data["port"] = 8080;

        auto err = validate(data, schema);
        REQUIRE_FALSE(err.has_value());
    }

    SECTION("using deprecated property returns error with description") {
        Dictionary data;
        data["server_port"] = 8080;

        auto err = validate(data, schema);
        REQUIRE(err.has_value());

        std::string msg = err.value();
        REQUIRE(msg.find("deprecated") != std::string::npos);
        REQUIRE(msg.find("server_port") != std::string::npos);
        REQUIRE(msg.find("Use 'port' instead") != std::string::npos);
    }
}

TEST_CASE("deprecated enum value in oneOf is reported", "[validate][deprecated][enum]") {
    Dictionary schema;
    schema["type"] = "object";

    Dictionary props;
    Dictionary bcProp;

    std::vector<Dictionary> alternatives;

    // Current BC type
    Dictionary currentBC;
    currentBC["const"] = "supersonic inflow";
    currentBC["description"] = "Supersonic inflow boundary condition";
    alternatives.push_back(currentBC);

    // Deprecated BC type
    Dictionary deprecatedBC;
    deprecatedBC["const"] = "dirichlet";
    deprecatedBC["deprecated"] = true;
    deprecatedBC["description"] =
                "DEPRECATED: Use 'supersonic inflow' instead. Renamed for clarity.";
    alternatives.push_back(deprecatedBC);

    bcProp["oneOf"] = alternatives;
    props["boundary_type"] = bcProp;

    schema["properties"] = props;

    SECTION("using current enum value passes") {
        Dictionary data;
        data["boundary_type"] = "supersonic inflow";

        auto err = validate(data, schema);
        REQUIRE_FALSE(err.has_value());
    }

    SECTION("using deprecated enum value returns error with description") {
        Dictionary data;
        data["boundary_type"] = "dirichlet";

        auto err = validate(data, schema);
        REQUIRE(err.has_value());

        std::string msg = err.value();
        REQUIRE(msg.find("deprecated") != std::string::npos);
        REQUIRE(msg.find("dirichlet") != std::string::npos);
        REQUIRE(msg.find("Use 'supersonic inflow' instead") != std::string::npos);
    }
}

TEST_CASE("deprecated object in anyOf with $ref is reported", "[validate][deprecated][anyOf]") {
    // Minimal test case based on the actual input.schema.json structure
    std::string schemaStr = R"({
        "type": "object",
        "definitions": {
            "Supersonic Outflow": {
                "type": "object",
                "properties": {
                    "type": {"type": "string", "const": "supersonic outflow"},
                    "mesh boundary tags": {"type": "integer"}
                },
                "additionalProperties": false
            },
            "Extrapolation": {
                "type": "object",
                "properties": {
                    "type": {"type": "string", "const": "extrapolation"},
                    "mesh boundary tags": {"type": "integer"}
                },
                "additionalProperties": false,
                "deprecated": true,
                "description": "DEPRECATED: Use <supersonic outflow> instead."
            }
        },
        "properties": {
            "boundary condition": {
                "anyOf": [
                    {"$ref": "#/definitions/Supersonic Outflow"},
                    {"$ref": "#/definitions/Extrapolation"}
                ]
            }
        }
    })";

    Dictionary schema = parse_json(schemaStr);

    SECTION("using current boundary condition passes validation") {
        std::string dataStr = R"({
            "boundary condition": {
                "type": "supersonic outflow",
                "mesh boundary tags": 1
            }
        })";
        Dictionary data = parse_json(dataStr);

        auto err = validate(data, schema);
        REQUIRE_FALSE(err.has_value());
    }

    SECTION("using deprecated boundary condition returns error") {
        std::string dataStr = R"({
            "boundary condition": {
                "type": "extrapolation",
                "mesh boundary tags": 1
            }
        })";
        Dictionary data = parse_json(dataStr);

        auto err = validate(data, schema);
        REQUIRE(err.has_value());

        std::string msg = err.value();
        REQUIRE(msg.find("deprecated") != std::string::npos);
        REQUIRE(msg.find("Use <supersonic outflow>") != std::string::npos);
    }
}

TEST_CASE("deprecated object in oneOf with $ref is reported", "[validate][deprecated][oneOf]") {
    std::string schemaStr = R"({
        "type": "object",
        "definitions": {
            "Fast Mode": {
                "type": "object",
                "properties": {
                    "mode": {"type": "string", "const": "fast"}
                },
                "additionalProperties": false
            },
            "Turbo Mode": {
                "type": "object",
                "properties": {
                    "mode": {"type": "string", "const": "turbo"}
                },
                "additionalProperties": false,
                "deprecated": true,
                "description": "DEPRECATED: Use 'fast' mode instead."
            }
        },
        "properties": {
            "mode config": {
                "oneOf": [
                    {"$ref": "#/definitions/Fast Mode"},
                    {"$ref": "#/definitions/Turbo Mode"}
                ]
            }
        }
    })";

    Dictionary schema = parse_json(schemaStr);

    SECTION("using current mode passes validation") {
        std::string dataStr = R"({"mode config": {"mode": "fast"}})";
        Dictionary data = parse_json(dataStr);

        auto err = validate(data, schema);
        REQUIRE_FALSE(err.has_value());
    }

    SECTION("using deprecated mode returns error") {
        std::string dataStr = R"({"mode config": {"mode": "turbo"}})";
        Dictionary data = parse_json(dataStr);

        auto err = validate(data, schema);
        REQUIRE(err.has_value());

        std::string msg = err.value();
        REQUIRE(msg.find("deprecated") != std::string::npos);
        REQUIRE(msg.find("Use 'fast' mode instead") != std::string::npos);
    }
}
