#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <ps/parsec.h>
#include <ps/validate.h>

TEST_CASE("unevaluatedProperties false rejects extra keys across allOf", "[validate][unit]") {
    const std::string schema_text = R"JSON(
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "type": "object",
  "properties": {
    "pets": {
      "type": "array",
      "items": {
        "allOf": [
          { "$ref": "#/definitions/pet" }
        ],
        "unevaluatedProperties": false
      }
    }
  },
  "definitions": {
    "pet": {
      "type": "object",
      "properties": {
        "leg count": { "type": "integer" },
        "color": { "type": "string" }
      }
    }
  }
}
)JSON";

    const std::string data_text = R"JSON(
{
  "pets": [
    {
      "leg count": 4,
      "blah": "should not be here",
      "color": "brown"
    }
  ]
}
)JSON";

    ps::Dictionary schema = ps::parse_json(schema_text);
    ps::Dictionary data = ps::parse_json(data_text);

    ps::ValidationResult result = ps::validate_all(data, schema, data_text);
    REQUIRE_FALSE(result.is_valid());

    const std::string formatted = result.format();
    REQUIRE_THAT(formatted, Catch::Matchers::ContainsSubstring("blah"));
}

TEST_CASE("unevaluatedProperties false allows only evaluated keys", "[validate][unit]") {
    const std::string schema_text = R"JSON(
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "type": "object",
  "properties": {
    "pets": {
      "type": "array",
      "items": {
        "allOf": [
          { "$ref": "#/definitions/pet" }
        ],
        "unevaluatedProperties": false
      }
    }
  },
  "definitions": {
    "pet": {
      "type": "object",
      "properties": {
        "leg count": { "type": "integer" },
        "color": { "type": "string" }
      }
    }
  }
}
)JSON";

    const std::string data_text = R"JSON(
{
  "pets": [
    {
      "leg count": 4,
      "color": "brown"
    }
  ]
}
)JSON";

    ps::Dictionary schema = ps::parse_json(schema_text);
    ps::Dictionary data = ps::parse_json(data_text);

    ps::ValidationResult result = ps::validate_all(data, schema, data_text);
    REQUIRE(result.is_valid());
}
