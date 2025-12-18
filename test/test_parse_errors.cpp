#include <catch2/catch_all.hpp>
#include <ps/json.h>

using namespace ps;

TEST_CASE("parse reports line and column in error messages") {
    // malformed string: unterminated string on line 2
    std::string s = R"(
    {"a": "unterminated
    )";
    try {
        parse_json(s);
        FAIL("expected parse to throw");
    } catch (const std::exception& e) {
        std::string msg = e.what();
        // message should mention line/column info or at least include 'unexpected end'
        REQUIRE((msg.find("unexpected end") != std::string::npos ||
                 msg.find(":") != std::string::npos));
    }
}

TEST_CASE("unmatched opener includes opener location") {
    // missing closing bracket for the object opened on line 1
    std::string s = R"({"obj": [1, 2, 3)";
    try {
        parse_json(s);
        FAIL("expected parse to throw");
    } catch (const std::exception& e) {
        std::string msg = e.what();
        // the parser includes opener location in messages for arrays/objects
        REQUIRE((msg.find("opened at") != std::string::npos));
    }
}

TEST_CASE("extra data after value is reported") {
    std::string s = "true false";
    try {
        parse_json(s);
        FAIL("expected parse to throw");
    } catch (const std::exception& e) {
        std::string msg = e.what();
        REQUIRE((msg.find("extra data after JSON value") != std::string::npos));
    }
}

TEST_CASE("Empty object is a valid json object") {
    std::string s = "{}";
    auto v = parse_json(s).dump();
    REQUIRE(v == "{}");
}

TEST_CASE("Automatically strip comments", "[comments]") {
    std::string full_config = R"(
# This is a comment
{
"all":{
  "your":6,
  "base":"#dog",
# is mine!!!!
  "nope":"my #base"
}
}
)";
    auto dict = parse_json(full_config);
    REQUIRE(dict.has("all"));
}

TEST_CASE("empty object is a valid json") {
    std::string s = "{}";
    auto v = parse_json(s);
    REQUIRE(v.isDict());
    REQUIRE(v.size() == 0);
}

TEST_CASE("missing a comment isn't an error in json parser") {
    std::string s = R"(
{
  "key1": "value" 
  "key2": "value" 
}
)";
    REQUIRE_NOTHROW(parse_json(s));
}

TEST_CASE("missing an extrac comma isn't an error in json parser") {
    std::string s = R"(
{
  "key1": "value" ,
  "key2": "value" ,
}
)";
    REQUIRE_NOTHROW(parse_json(s));
}

TEST_CASE("Duplicate keys throw an exception") {
    std::string s = R"(
{
  "key1": "value1",
  "key2": "value2",
  "key1": "value3"
}
)";
    try {
        parse_json(s);
        FAIL("expected parse to throw");
    } catch (const std::exception& e) {
        std::string msg = e.what();
        REQUIRE((msg.find("duplicate key") != std::string::npos));
    }
}

TEST_CASE("An empty string is not a parse error") {
    std::string s = "";
    auto dict = parse_json(s);
}

TEST_CASE("Extra trailing closing brace is not an error ") {
    std::string s = R"(
{
  "key1": "value1",
  "key2": "value2"
}}
)";
    auto dict = parse_json(s);
}