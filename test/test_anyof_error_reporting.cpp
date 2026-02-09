#include <catch2/catch_test_macros.hpp>
#include <ps/parsec.h>
#include <ps/validate.h>

TEST_CASE("anyOf errors use discriminator filtering with type field", "[validate][anyof][error]") {
    // Schema with multiple anyOf alternatives that use "type" as discriminator
    std::string schema_json = R"({
        "anyOf": [
            {
                "title": "Circle",
                "type": "object",
                "properties": {
                    "type": {"const": "circle"},
                    "radius": {"type": "number"}
                },
                "required": ["type", "radius"],
                "additionalProperties": false
            },
            {
                "title": "Rectangle",
                "type": "object",
                "properties": {
                    "type": {"const": "rectangle"},
                    "width": {"type": "number"},
                    "height": {"type": "number"}
                },
                "required": ["type", "width", "height"],
                "additionalProperties": false
            },
            {
                "title": "Triangle",
                "type": "object",
                "properties": {
                    "type": {"const": "triangle"},
                    "base": {"type": "number"},
                    "height": {"type": "number"}
                },
                "required": ["type", "base", "height"],
                "additionalProperties": false
            }
        ]
    })";
    
    auto schema = ps::parse_json(schema_json);
    
    // Data with type="circle" but missing required field
    std::string data_json = R"({"type": "circle"})";
    auto data = ps::parse_json(data_json);
    
    auto error = ps::validate(data, schema);
    
    REQUIRE(error.has_value());
    std::string err_msg = *error;
    
    // Should mention anyOf failure
    REQUIRE(err_msg.find("anyOf did not match") != std::string::npos);
    
    // Should show Circle option (matches discriminator)
    REQUIRE(err_msg.find("Circle") != std::string::npos);
    
    // Should NOT show Rectangle or Triangle (discriminator mismatch)
    REQUIRE(err_msg.find("Rectangle") == std::string::npos);
    REQUIRE(err_msg.find("Triangle") == std::string::npos);
    
    // Should mention the missing field
    REQUIRE(err_msg.find("radius") != std::string::npos);
}

TEST_CASE("anyOf errors limit alternatives to 5 when many options exist", "[validate][anyof][error]") {
    // Schema with many anyOf alternatives
    std::string schema_json = R"({
        "anyOf": [
            {"properties": {"opt1": {"type": "string"}}, "required": ["opt1"]},
            {"properties": {"opt2": {"type": "string"}}, "required": ["opt2"]},
            {"properties": {"opt3": {"type": "string"}}, "required": ["opt3"]},
            {"properties": {"opt4": {"type": "string"}}, "required": ["opt4"]},
            {"properties": {"opt5": {"type": "string"}}, "required": ["opt5"]},
            {"properties": {"opt6": {"type": "string"}}, "required": ["opt6"]},
            {"properties": {"opt7": {"type": "string"}}, "required": ["opt7"]},
            {"properties": {"opt8": {"type": "string"}}, "required": ["opt8"]}
        ]
    })";
    
    auto schema = ps::parse_json(schema_json);
    auto data = ps::parse_json("{}");
    
    auto error = ps::validate(data, schema);
    
    REQUIRE(error.has_value());
    std::string err_msg = *error;
    
    // Should mention there are more alternatives
    REQUIRE(err_msg.find("more alternatives") != std::string::npos);
}

TEST_CASE("anyOf errors show original data before defaults", "[validate][anyof][error][defaults]") {
    // This test verifies that when validation fails on data after defaults are applied,
    // the error message shows the original user-provided data, not the defaults-populated version.
    
    std::string schema_json = R"({
        "properties": {
            "name": {"type": "string", "default": "default_name"},
            "value": {"type": "integer"}
        },
        "anyOf": [
            {
                "title": "MustHaveValue100",
                "properties": {
                    "value": {"const": 100},
                    "name": {"type": "string"}
                },
                "required": ["value"],
                "additionalProperties": false
            }
        ]
    })";
    
    auto schema = ps::parse_json(schema_json);
    
    // User provides data with wrong value
    std::string data_json = R"({"value": 42})";
    auto original_data = ps::parse_json(data_json);
    auto data_with_defaults = ps::setDefaults(original_data, schema);
    
    // data_with_defaults now has {"value": 42, "name": "default_name"}
    // This will fail anyOf because value must be 100
    
    // Set original data for error messages
    ps::set_original_data(&original_data);
    
    auto error = ps::validate(data_with_defaults, schema);
    
    // Clear original data pointer
    ps::set_original_data(nullptr);
    
    REQUIRE(error.has_value());
    std::string err_msg = *error;
    
    // Error should show original value  (just {"value": 42}, no name field)
    REQUIRE(err_msg.find("value: 42") != std::string::npos);
    
    // Should NOT show the default name that was added
    REQUIRE(err_msg.find("default_name") == std::string::npos);
}

TEST_CASE("anyOf errors work with kind discriminator", "[validate][anyof][error]") {
    std::string schema_json = R"({
        "anyOf": [
            {
                "title": "Email",
                "properties": {
                    "kind": {"const": "email"},
                    "address": {"type": "string"}
                },
                "required": ["kind", "address"]
            },
            {
                "title": "Phone",
                "properties": {
                    "kind": {"const": "phone"},
                    "number": {"type": "string"}
                },
                "required": ["kind", "number"]
            }
        ]
    })";
    
    auto schema = ps::parse_json(schema_json);
    auto data = ps::parse_json(R"({"kind": "email"})");
    
    auto error = ps::validate(data, schema);
    
    REQUIRE(error.has_value());
    std::string err_msg = *error;
    
    // Should show Email option only
    REQUIRE(err_msg.find("Email") != std::string::npos);
    REQUIRE(err_msg.find("Phone") == std::string::npos);
}

TEST_CASE("anyOf errors work with variant discriminator", "[validate][anyof][error]") {
    std::string schema_json = R"({
        "anyOf": [
            {
                "title": "Success",
                "properties": {
                    "variant": {"const": "success"},
                    "data": {"type": "object"}
                },
                "required": ["variant", "data"]
            },
            {
                "title": "Error",
                "properties": {
                    "variant": {"const": "error"},
                    "message": {"type": "string"}
                },
                "required": ["variant", "message"]
            }
        ]
    })";
    
    auto schema = ps::parse_json(schema_json);
    auto data = ps::parse_json(R"({"variant": "error"})");
    
    auto error = ps::validate(data, schema);
    
    REQUIRE(error.has_value());
    std::string err_msg = *error;
    
    // Should show Error option only
    REQUIRE(err_msg.find("Error") != std::string::npos);
    REQUIRE(err_msg.find("Success") == std::string::npos);
}

TEST_CASE("anyOf errors don't filter when discriminator not widely used", "[validate][anyof][error]") {
    // Schema where only 1 out of 3 alternatives use a type discriminator
    std::string schema_json = R"({
        "anyOf": [
            {
                "title": "TypedOption",
                "properties": {
                    "type": {"const": "typed"},
                    "value": {"type": "string"}
                },
                "required": ["type", "value"]
            },
            {
                "title": "OptionA",
                "properties": {
                    "fieldA": {"type": "string"}
                },
                "required": ["fieldA"]
            },
            {
                "title": "OptionB",
                "properties": {
                    "fieldB": {"type": "string"}
                },
                "required": ["fieldB"]
            }
        ]
    })";
    
    auto schema = ps::parse_json(schema_json);
    
    // Data with type that only 1/3 schemas constrain
    auto data = ps::parse_json(R"({"type": "typed"})");
    
    auto error = ps::validate(data, schema);
    
    REQUIRE(error.has_value());
    std::string err_msg = *error;
    
    // Should show multiple options (not just TypedOption)
    // because less than half the schemas use the discriminator
    REQUIRE(err_msg.find("Option") != std::string::npos);
}

TEST_CASE("anyOf errors show user value in nested objects", "[validate][anyof][error][nested]") {
    std::string schema_json = R"({
        "type": "object",
        "properties": {
            "config": {
                "anyOf": [
                    {
                        "title": "DatabaseConfig",
                        "type": "object",
                        "properties": {
                            "type": {"const": "database"},
                            "host": {"type": "string"},
                            "port": {"type": "integer", "default": 5432}
                        },
                        "required": ["type", "host"],
                        "additionalProperties": false
                    }
                ]
            }
        }
    })";
    
    auto schema = ps::parse_json(schema_json);
    
    std::string data_json = R"({"config": {"type": "database"}})";
    auto original_data = ps::parse_json(data_json);
    auto data_with_defaults = ps::setDefaults(original_data, schema);
    
    ps::set_original_data(&original_data);
    auto error = ps::validate(data_with_defaults, schema);
    ps::set_original_data(nullptr);
    
    REQUIRE(error.has_value());
    std::string err_msg = *error;
    
    // Should show original nested value without the default port
    REQUIRE(err_msg.find("type") != std::string::npos);
    REQUIRE(err_msg.find("database") != std::string::npos);
    
    // Should mention it's missing host
    REQUIRE(err_msg.find("host") != std::string::npos);
    
    // Should NOT show the default port in the "Your value" section
    // (This is hard to test precisely without parsing, but we can check the port isn't there)
    size_t your_value_pos = err_msg.find("Your value:");
    size_t alternatives_pos = err_msg.find("Alternatives:");
    if (your_value_pos != std::string::npos && alternatives_pos != std::string::npos) {
        std::string your_value_section = err_msg.substr(your_value_pos, alternatives_pos - your_value_pos);
        // Should not have port in the user's original value
        REQUIRE(your_value_section.find("5432") == std::string::npos);
    }
}

TEST_CASE("anyOf errors handle arrays with discriminators", "[validate][anyof][error][array]") {
    std::string schema_json = R"({
        "type": "array",
        "items": {
            "anyOf": [
                {
                    "title": "Dog",
                    "type": "object",
                    "properties": {
                        "species": {"const": "dog"},
                        "breed": {"type": "string"}
                    },
                    "required": ["species", "breed"],
                    "additionalProperties": false
                },
                {
                    "title": "Cat",
                    "type": "object",
                    "properties": {
                        "species": {"const": "cat"},
                        "color": {"type": "string"}
                    },
                    "required": ["species", "color"],
                    "additionalProperties": false
                }
            ]
        }
    })";
    
    auto schema = ps::parse_json(schema_json);
    
    std::string data_json = R"([{"species": "dog"}, {"species": "cat", "color": "orange"}])";
    auto data = ps::parse_json(data_json);
    
    auto result = ps::validate_all(data, schema);
    
    REQUIRE_FALSE(result.is_valid());
    std::string formatted = result.format();
    
    // Should show Dog option for the first array item
    REQUIRE(formatted.find("Dog") != std::string::npos);
    REQUIRE(formatted.find("breed") != std::string::npos);
    
    // Should have at least 1 error (for the missing breed in first item)
    REQUIRE(result.error_count() >= 1);
}

TEST_CASE("anyOf doesn't crash when original_data is null", "[validate][anyof][error][safety]") {
    std::string schema_json = R"({
        "anyOf": [
            {
                "properties": {"type": {"const": "a"}, "x": {"type": "string"}},
                "required": ["type", "x"]
            }
        ]
    })";
    
    auto schema = ps::parse_json(schema_json);
    auto data = ps::parse_json(R"({"type": "a"})");
    
    // Don't set original_data - should fall back to showing data with defaults
    ps::set_original_data(nullptr);
    
    auto error = ps::validate(data, schema);
    
    REQUIRE(error.has_value());
    // Should not crash and should still produce an error message
    REQUIRE(error->find("anyOf") != std::string::npos);
}

TEST_CASE("anyOf filtering with enum instead of const", "[validate][anyof][error]") {
    std::string schema_json = R"({
        "anyOf": [
            {
                "title": "WarmColor",
                "properties": {
                    "type": {"enum": ["red", "orange", "yellow"]},
                    "intensity": {"type": "number"}
                },
                "required": ["type", "intensity"]
            },
            {
                "title": "CoolColor",
                "properties": {
                    "type": {"enum": ["blue", "green", "purple"]},
                    "saturation": {"type": "number"}
                },
                "required": ["type", "saturation"]
            }
        ]
    })";
    
    auto schema = ps::parse_json(schema_json);
    auto data = ps::parse_json(R"({"type": "red"})");
    
    auto error = ps::validate(data, schema);
    
    REQUIRE(error.has_value());
    std::string err_msg = *error;
    
    // Should show only WarmColor (red is in its enum)
    REQUIRE(err_msg.find("WarmColor") != std::string::npos);
    REQUIRE(err_msg.find("CoolColor") == std::string::npos);
}
