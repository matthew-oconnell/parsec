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
    std::string expected = R"({
  "level1": {
    "level2": {
      "level3": {
        "level4": {
          "level5": {
            "level6": {
              "level7": {
                "level8": {
                  "level9": {
                    "value": 42
                  }
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