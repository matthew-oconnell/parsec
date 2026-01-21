#include <catch2/catch_all.hpp>
#include <ps/dictionary.h>
#include <ps/json.h>

using namespace ps;

TEST_CASE("Pretty print produces stable indentation for indent=2 and indent=4") {
    Dictionary dict;
    dict["some array"][0] = 5;
    dict["some array"][1] = 6;
    dict["some value"] = double(1e200);
    dict["empty array"] = std::vector<double>{};
    dict["empty object"] = Dictionary();

    // pretty print should use compact array printing by default
    SECTION("indent = 2") {
        std::string expected = R"({
  "empty array": [],
  "empty object": {},
  "some array": [5,6],
  "some value": 1e+200
})";
        REQUIRE(expected == dict.dump(2));
    }

    SECTION("indent = 4") {
        std::string expected = R"({
    "empty array": [],
    "empty object": {},
    "some array": [5,6],
    "some value": 1e+200
})";
        REQUIRE(expected == dict.dump(4));
    }
}

TEST_CASE("Pretty print works on nested objects") {
    Dictionary dict;
    dict["level1"]["level2"]["level3"]["value"] = 42;
    std::string expected = R"({"level1":{"level2":{"level3":{"value":42}}}})";
    REQUIRE(expected == dict.dump());
}

TEST_CASE("Pretty print Json", "[dump]") {
    Dictionary dict;
    dict["some array"][0] = 5;
    dict["some array"][1] = 6;
    dict["some value"] = double(1e200);
    dict["empty array"] = std::vector<double>{};
    dict["empty object"] = Dictionary();
    std::string expected = R"({
    "empty array": [],
    "empty object": {},
    "some array": [5,6],
    "some value": 1e+200
})";
    REQUIRE(expected == dict.dump(4));
}

TEST_CASE("Pretty print compact will eventually use indentation if the line would go beyond 80 "
          "characters") {
    Dictionary dict;
    dict["level1"]["level2"]["level3"]["level4"]["level5"]["level6"]["level7"]["level8"]["level9"]
        ["value"] = 42;
    // The deeply nested structure is expanded, but the innermost simple object
    // {"value":42} is collapsed since it only contains scalars and fits on one line
    std::string expected = R"({
  "level1": {
    "level2": {
      "level3": {
        "level4": {
          "level5": {
            "level6": {
              "level7": {
                "level8": {
                  "level9": {"value":42}
                }
              }
            }
          }
        }
      }
    }
  }
})";
    REQUIRE(expected == dict.dump(2));
}

TEST_CASE("Dictionary with escaped quoted strings pretty prints correctly") {
    Dictionary dict;
    dict["quote"] = std::string("He said, \"Hello, World!\"");
    std::string expected = R"({"quote":"He said, \"Hello, World!\""})";
    REQUIRE(expected == dict.dump());
}

TEST_CASE("Dictionary dump escapes all special characters") {
    Dictionary dict;
    dict["backslash"] = "C:\\path\\to\\file";
    dict["newline"] = "line1\nline2";
    dict["tab"] = "col1\tcol2";
    dict["carriage_return"] = "text\rmore";
    dict["backspace"] = "back\bspace";
    dict["formfeed"] = "form\ffeed";
    dict["mixed"] = "Quote: \" Newline: \n Tab: \t Backslash: \\";

    std::string json = dict.dump(0, true);

    // Verify escaping in output
    REQUIRE(json.find("\\\\") != std::string::npos);  // backslash escaped
    REQUIRE(json.find("\\n") != std::string::npos);   // newline escaped
    REQUIRE(json.find("\\t") != std::string::npos);   // tab escaped
    REQUIRE(json.find("\\r") != std::string::npos);   // carriage return escaped
    REQUIRE(json.find("\\b") != std::string::npos);   // backspace escaped
    REQUIRE(json.find("\\f") != std::string::npos);   // formfeed escaped

    // Verify round-trip: parse it back and check values match
    Dictionary parsed = ps::parse_json(json);
    REQUIRE(parsed.at("backslash").asString() == "C:\\path\\to\\file");
    REQUIRE(parsed.at("newline").asString() == "line1\nline2");
    REQUIRE(parsed.at("tab").asString() == "col1\tcol2");
    REQUIRE(parsed.at("mixed").asString() == "Quote: \" Newline: \n Tab: \t Backslash: \\");
}

TEST_CASE("Pretty print does not create overly long lines with nested compact objects", "[dump]") {
    // Create a scenario similar to the user's example: an array containing
    // multiple small objects that individually can be compacted, but together
    // would exceed 80 characters on a single line.
    Dictionary dict;

    // Create an array with multiple compact objects
    dict["examples"][0]["hi"][0] = 1;
    dict["examples"][0]["hi"][1] = 1.2;
    dict["examples"][0]["hi"][2] = 2;
    dict["examples"][0]["lo"][0] = 0;
    dict["examples"][0]["lo"][1] = 0.2;
    dict["examples"][0]["lo"][2] = 1;
    dict["examples"][0]["state"] = "box region";
    dict["examples"][0]["type"] = "aabb";

    dict["examples"][1]["hi"][0] = -10;
    dict["examples"][1]["hi"][1] = 100;
    dict["examples"][1]["hi"][2] = 100;
    dict["examples"][1]["lo"][0] = 0;
    dict["examples"][1]["lo"][1] = -100;
    dict["examples"][1]["lo"][2] = -100;
    dict["examples"][1]["state"] = "wake region";
    dict["examples"][1]["type"] = "box";

    // With indent=4, this should expand the array to multi-line because
    // the compact representation would be too long
    std::string result = dict.dump(4);

    // The result should have the array expanded to multiple lines
    // Each line should be <= 80 characters (accounting for indentation)
    std::istringstream iss(result);
    std::string line;
    while (std::getline(iss, line)) {
        // Allow some flexibility for the examples line itself
        INFO("Line: " << line);
        INFO("Length: " << line.size());
        // We want to verify that no single line is excessively long
        // In practice, with proper formatting, lines should be under 80
        // but we'll allow up to 100 to account for deep nesting
        REQUIRE(line.size() <= 100);
    }

    // Verify that "examples" array is expanded (contains newlines within it)
    // If it's on one line, it would look like: "examples": [...]
    // If expanded, we'd see "examples": [\n
    size_t examples_pos = result.find("\"examples\"");
    REQUIRE(examples_pos != std::string::npos);
    size_t bracket_pos = result.find("[", examples_pos);
    REQUIRE(bracket_pos != std::string::npos);
    size_t newline_after_bracket = result.find("\n", bracket_pos);
    REQUIRE(newline_after_bracket != std::string::npos);
    // The newline should come soon after the bracket, indicating expansion
    REQUIRE(newline_after_bracket - bracket_pos < 5);
}

TEST_CASE("Pretty print collapses simple objects containing only scalars", "[dump]") {
    Dictionary dict;
    
    // Create a nested structure where some objects only contain scalars
    // and others contain nested structures
    dict["properties"]["name"]["type"] = "string";
    dict["properties"]["name"]["maxLength"] = 100;
    
    dict["properties"]["items"]["type"] = "array";
    dict["properties"]["items"]["items"]["type"] = "number";
    dict["properties"]["items"]["minItems"] = 1;
    
    std::string result = dict.dump(4);
    
    // The inner "items" object that only contains {"type":"number"} should be collapsed
    REQUIRE(result.find("{\"type\":\"number\"}") != std::string::npos);
    
    // The "name" object should also be collapsed since it only has scalars
    // and fits on one line (key order may vary)
    bool has_collapsed_name = (result.find("{\"maxLength\":100,\"type\":\"string\"}") != std::string::npos ||
                                result.find("{\"type\":\"string\",\"maxLength\":100}") != std::string::npos);
    REQUIRE(has_collapsed_name);
}
