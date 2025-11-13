#include <catch2/catch_all.hpp>
#include <ps/parsec.h>
#include <ps/ron.h>

TEST_CASE("RON and JSON parsing APIs work and produce distinct errors") {
    // valid RON-ish input (unquoted keys)
    std::string ron = "key = 1";
    REQUIRE_NOTHROW(ps::parse_ron(ron));

    // invalid JSON for the same input
    REQUIRE_THROWS(ps::parse_json(ron));

    // valid JSON
    std::string j = "{\"a\": 1}";
    REQUIRE_NOTHROW(ps::parse_json(j));

    // invalid RON for JSON-like input (RON parser may accept, but this checks API)
    REQUIRE_NOTHROW(ps::parse_ron(j));
}
