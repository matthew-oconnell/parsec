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

TEST_CASE("parse() respects vim modeline format hint", "[parse][hint]") {
    std::string toml_with_hint = R"(# vim: set filetype=toml:
title = "Test"
port = 8080
)";
    auto d = ps::parse(toml_with_hint);
    REQUIRE(d.at("title").asString() == "Test");
    REQUIRE(d.at("port").asInt() == 8080);
}

TEST_CASE("parse() respects vim ft= format hint", "[parse][hint]") {
    std::string ini_with_hint = R"(# vim: ft=ini
[section]
key = value
)";
    auto d = ps::parse(ini_with_hint);
    REQUIRE(d.at("section").at("key").asString() == "value");
}

TEST_CASE("parse() respects emacs mode line format hint", "[parse][hint]") {
    std::string json_with_hint = R"(# -*- mode: json -*-
{"name": "test", "value": 42}
)";
    auto d = ps::parse(json_with_hint);
    REQUIRE(d.at("name").asString() == "test");
    REQUIRE(d.at("value").asInt() == 42);
}

TEST_CASE("parse() respects simple format: pragma", "[parse][hint]") {
    std::string yaml_with_hint = R"(# format: yaml
name: test
items:
  - first
  - second
)";
    auto d = ps::parse(yaml_with_hint);
    REQUIRE(d.at("name").asString() == "test");
    REQUIRE(d.at("items").size() == 2);
}

TEST_CASE("parse() format hint is case-insensitive", "[parse][hint]") {
    std::string toml_hint = "# format: TOML\ntitle = \"Test\"\n";
    auto d = ps::parse(toml_hint);
    REQUIRE(d.at("title").asString() == "Test");
}

TEST_CASE("parse() reports error from hinted format when hint is wrong", "[parse][hint]") {
    // Says it's JSON but it's actually TOML syntax
    std::string toml_with_wrong_hint = R"(# format: json
title = "This is TOML"
port = 8080
)";
    try {
        ps::parse(toml_with_wrong_hint);
        FAIL("Expected parse to throw JSON error");
    } catch (const std::exception& e) {
        std::string msg = e.what();
        // Should get error from JSON parser (invalid literal, parse error, etc.)
        // NOT succeed by falling back to TOML
        REQUIRE((msg.find("invalid literal") != std::string::npos || 
                 msg.find("parse error") != std::string::npos ||
                 msg.find("unexpected") != std::string::npos));
    }
}

TEST_CASE("parse() accepts yml as alias for yaml", "[parse][hint]") {
    std::string yaml_hint = R"(# format: yml
name: test
count: 5
)";
    auto d = ps::parse(yaml_hint);
    REQUIRE(d.at("name").asString() == "test");
    REQUIRE(d.at("count").asInt() == 5);
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
