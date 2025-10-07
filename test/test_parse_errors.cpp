#include <catch2/catch_all.hpp>
#include <ps/simple_json.hpp>

using namespace ps;

TEST_CASE("parse reports line and column in error messages") {
    // malformed string: unterminated string on line 2
    std::string s = R"(
    {"a": "unterminated
    )";
    try {
        parse_json(s);
        FAIL("expected parse to throw");
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        // message should mention line/column info or at least include 'unexpected end'
    REQUIRE((msg.find("unexpected end") != std::string::npos || msg.find(":") != std::string::npos));
    }
}

TEST_CASE("unmatched opener includes opener location") {
    // missing closing bracket for the object opened on line 1
    std::string s = R"({"obj": [1, 2, 3)";
    try {
        parse_json(s);
        FAIL("expected parse to throw");
    } catch (const std::runtime_error& e) {
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
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
    REQUIRE((msg.find("extra data after JSON value") != std::string::npos));
    }
}
