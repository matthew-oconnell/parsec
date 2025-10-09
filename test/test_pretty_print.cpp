#include <catch2/catch_all.hpp>
#include <ps/dictionary.hpp>

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
  "some array": [5, 6],
  "some value": 1e+200
})";
        REQUIRE(expected == dict.dump(2));
    }

    SECTION("indent = 4") {
        std::string expected = R"({
    "empty array": [],
    "empty object": {},
    "some array": [5, 6],
    "some value": 1e+200
})";
        REQUIRE(expected == dict.dump(4));
    }
}

TEST_CASE("Pretty print works on nested objects") {
    Dictionary dict;
    dict["level1"]["level2"]["level3"]["value"] = 42;
    std::string expected = R"({ "level1": { "level2": { "level3": { "value": 42 } } } })";
    REQUIRE(expected == dict.dump(4));
}

TEST_CASE("Pretty print compact will eventually use indentation if the line would go beyond 80 characters") {
    Dictionary dict;
    dict["level1"]["level2"]["level3"]["level4"]["level5"]["level6"]["level7"]["level8"]["level9"]["value"] = 42;
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