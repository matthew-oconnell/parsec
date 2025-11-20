#include <catch2/catch_test_macros.hpp>
#include <ps/parse.h>
#include <string>

TEST_CASE("parse() tries all formats - JSON succeeds", "[parse]") {
    std::string json = R"({"key": "value", "number": 42})";
    auto d = ps::parse(json);
    REQUIRE(d.at("key").asString() == "value");
    REQUIRE(d.at("number").asInt() == 42);
}

TEST_CASE("parse() tries all formats - RON succeeds", "[parse]") {
    std::string ron = R"({key: value, number: 42})";
    auto d = ps::parse(ron);
    REQUIRE(d.at("key").asString() == "value");
    REQUIRE(d.at("number").asInt() == 42);
}

TEST_CASE("parse() tries all formats - TOML succeeds", "[parse]") {
    std::string toml = R"(
key = "value"
number = 42
)";
    auto d = ps::parse(toml);
    REQUIRE(d.at("key").asString() == "value");
    REQUIRE(d.at("number").asInt() == 42);
}

TEST_CASE("parse() tries all formats - INI succeeds", "[parse]") {
    std::string ini = R"(
key = value
number = 42
)";
    auto d = ps::parse(ini);
    // INI at root level (no section)
    REQUIRE(d.at("key").asString() == "value");
    REQUIRE(d.at("number").asInt() == 42);
}

TEST_CASE("parse() guesses format on parse failure", "[parse]") {
    // Test that when parsing fails, we report the most likely format
    // Use the "extra data after value" error which JSON parser does catch
    std::string bad_json = R"(true false)";
    try {
        ps::parse(bad_json);
        FAIL("Expected parse to throw");
    } catch (const std::exception& e) {
        std::string msg = e.what();
        REQUIRE(msg.find("Most likely intended format: JSON") != std::string::npos);
        REQUIRE(msg.find("extra data after JSON value") != std::string::npos);
    }
}

TEST_CASE("parse() guesses INI for INI with bad section", "[parse]") {
    std::string bad_ini = "[section without closing bracket\nkey = value";
    try {
        ps::parse(bad_ini);
        FAIL("Expected parse to throw");
    } catch (const std::exception& e) {
        std::string msg = e.what();
        REQUIRE(msg.find("Most likely intended format:") != std::string::npos);
    }
}

TEST_CASE("parse() provides helpful error for completely invalid input", "[parse]") {
    std::string garbage = "this is not valid in any format @#$%";
    try {
        ps::parse(garbage);
        FAIL("Expected parse to throw");
    } catch (const std::exception& e) {
        std::string msg = e.what();
        REQUIRE(msg.find("Failed to parse as any supported format") != std::string::npos);
        REQUIRE(msg.find("Most likely intended format:") != std::string::npos);
    }
}
