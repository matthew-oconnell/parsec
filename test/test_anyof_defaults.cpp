#include <catch2/catch_test_macros.hpp>
#include <ps/parsec.h>
#include <ps/validate.h>

TEST_CASE("Array dump() returns correct JSON for arrays at top level", "[dictionary][unit][dump]") {
    // Bug: dump() was calling dumpObject() instead of dumpValue(), causing arrays to print as {}
    ps::Dictionary arr;
    arr[0] = "first";
    arr[1] = "second";
    arr[2] = "third";

    std::string result = arr.dump(0, true);
    REQUIRE(result == R"(["first","second","third"])");
}

TEST_CASE("Array dump() with indent returns formatted array", "[dictionary][unit][dump]") {
    ps::Dictionary arr;
    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;

    std::string result = arr.dump(4, false);
    REQUIRE(result.find("[") != std::string::npos);
    REQUIRE(result.find("1") != std::string::npos);
    REQUIRE(result.find("2") != std::string::npos);
    REQUIRE(result.find("3") != std::string::npos);
    // Should not be an empty object
    REQUIRE(result != "{}");
}

TEST_CASE("Object array dump() returns correct JSON", "[dictionary][unit][dump]") {
    ps::Dictionary arr;

    ps::Dictionary obj1;
    obj1["name"] = "Alice";
    obj1["age"] = 30;
    arr[0] = obj1;

    ps::Dictionary obj2;
    obj2["name"] = "Bob";
    obj2["age"] = 25;
    arr[1] = obj2;

    std::string result = arr.dump(0, true);
    REQUIRE(result.find("Alice") != std::string::npos);
    REQUIRE(result.find("Bob") != std::string::npos);
    REQUIRE(result != "{}");
    REQUIRE(result[0] == '[');
}

TEST_CASE("setDefaults applies defaults from anyOf schemas", "[validate][unit][anyof]") {
    // Schema with anyOf - like the Visualization schema
    std::string schema_json = R"({
        "type": "object",
        "properties": {
            "items": {
                "type": "array",
                "items": {
                    "anyOf": [
                        {
                            "type": "object",
                            "properties": {
                                "type": {
                                    "type": "string",
                                    "enum": ["circle"]
                                },
                                "radius": {
                                    "type": "number",
                                    "default": 1.0
                                },
                                "color": {
                                    "type": "string",
                                    "default": "red"
                                }
                            }
                        },
                        {
                            "type": "object",
                            "properties": {
                                "type": {
                                    "type": "string",
                                    "enum": ["square"]
                                },
                                "size": {
                                    "type": "number",
                                    "default": 2.0
                                },
                                "color": {
                                    "type": "string",
                                    "default": "blue"
                                }
                            }
                        }
                    ]
                }
            }
        }
    })";

    std::string data_json = R"({
        "items": [
            {"type": "circle"},
            {"type": "square"}
        ]
    })";

    auto schema = ps::parse_json(schema_json);
    auto data = ps::parse_json(data_json);

    auto result = ps::setDefaults(data, schema);

    // Check that defaults were applied to the circle
    REQUIRE(result.has("items"));
    REQUIRE(result["items"][0].has("radius"));
    REQUIRE(result["items"][0]["radius"].asDouble() == 1.0);
    REQUIRE(result["items"][0].has("color"));
    REQUIRE(result["items"][0]["color"].asString() == "red");

    // Check that defaults were applied to the square
    REQUIRE(result["items"][1].has("size"));
    REQUIRE(result["items"][1]["size"].asDouble() == 2.0);
    REQUIRE(result["items"][1].has("color"));
    REQUIRE(result["items"][1]["color"].asString() == "blue");
}

TEST_CASE("setDefaults applies default from property with $ref", "[validate][unit][defaults]") {
    // Bug: when a property has both $ref and default, the default was lost after $ref resolution
    std::string schema_json = R"({
        "type": "object",
        "definitions": {
            "StringOrArray": {
                "oneOf": [
                    {"type": "string"},
                    {"type": "array", "items": {"type": "string"}}
                ]
            }
        },
        "properties": {
            "fields": {
                "$ref": "#/definitions/StringOrArray",
                "default": "auto"
            },
            "name": {
                "type": "string"
            }
        }
    })";

    std::string data_json = R"({
        "name": "test"
    })";

    auto schema = ps::parse_json(schema_json);
    auto data = ps::parse_json(data_json);

    auto result = ps::setDefaults(data, schema);

    // The "fields" key should have the default value applied
    REQUIRE(result.has("fields"));
    REQUIRE(result["fields"].asString() == "auto");
}

TEST_CASE("setDefaults handles nested anyOf with $ref and defaults", "[validate][unit][anyof]") {
    // Complex schema similar to the real Visualization schema
    std::string schema_json = R"({
        "type": "object",
        "definitions": {
            "FieldNames": {
                "oneOf": [
                    {"type": "string"},
                    {"type": "array", "items": {"type": "string"}}
                ]
            },
            "VolumeSample": {
                "type": "object",
                "properties": {
                    "type": {
                        "type": "string",
                        "enum": ["volume"]
                    },
                    "filename": {
                        "type": "string"
                    },
                    "fields": {
                        "$ref": "#/definitions/FieldNames",
                        "default": "auto"
                    },
                    "movie": {
                        "type": "boolean",
                        "default": false
                    }
                }
            },
            "PlaneSample": {
                "type": "object",
                "properties": {
                    "type": {
                        "type": "string",
                        "enum": ["plane"]
                    },
                    "filename": {
                        "type": "string"
                    },
                    "fields": {
                        "$ref": "#/definitions/FieldNames",
                        "default": "auto"
                    },
                    "normal": {
                        "type": "array"
                    },
                    "crinkle": {
                        "type": "boolean",
                        "default": false
                    }
                }
            }
        },
        "properties": {
            "visualization": {
                "type": "array",
                "items": {
                    "anyOf": [
                        {"$ref": "#/definitions/VolumeSample"},
                        {"$ref": "#/definitions/PlaneSample"}
                    ]
                }
            }
        }
    })";

    std::string data_json = R"({
        "visualization": [
            {"type": "volume", "filename": "domain.vtk"},
            {"type": "plane", "filename": "plane.vtk", "normal": [0, 1, 0]}
        ]
    })";

    auto schema = ps::parse_json(schema_json);
    auto data = ps::parse_json(data_json);

    auto result = ps::setDefaults(data, schema);

    // Both visualization items should have "fields": "auto" applied
    REQUIRE(result.has("visualization"));
    REQUIRE(result["visualization"][0].has("fields"));
    REQUIRE(result["visualization"][0]["fields"].asString() == "auto");
    REQUIRE(result["visualization"][0].has("movie"));
    REQUIRE(result["visualization"][0]["movie"].asBool() == false);

    REQUIRE(result["visualization"][1].has("fields"));
    REQUIRE(result["visualization"][1]["fields"].asString() == "auto");
    REQUIRE(result["visualization"][1].has("crinkle"));
    REQUIRE(result["visualization"][1]["crinkle"].asBool() == false);
}

TEST_CASE("Empty object dump() returns {}", "[dictionary][unit][dump]") {
    ps::Dictionary obj;
    REQUIRE(obj.dump(0, true) == "{}");
}

TEST_CASE("Nested array in object dumps correctly", "[dictionary][unit][dump]") {
    ps::Dictionary obj;
    obj["data"][0] = 1;
    obj["data"][1] = 2;
    obj["data"][2] = 3;

    std::string result = obj.dump(0, true);
    REQUIRE(result.find("\"data\":[1,2,3]") != std::string::npos);
}
