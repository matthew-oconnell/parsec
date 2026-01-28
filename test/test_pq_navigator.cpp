#include <catch2/catch_test_macros.hpp>
#include <ps/pq/navigator.h>
#include <ps/parsec.h>

// Phase 2.1 - Navigate Simple Paths

TEST_CASE("Navigate to nested value", "[pq][navigator][unit]") {
    ps::Dictionary d;
    d["server"]["port"] = 8080;
    
    ps::pq::Navigator nav;
    ps::pq::PathParser parser;
    auto tokens = parser.parse("server/port");
    
    auto result = nav.navigate(d, tokens);
    REQUIRE(result.asInt() == 8080);
}

TEST_CASE("Navigate to top-level value", "[pq][navigator][unit]") {
    ps::Dictionary d;
    d["name"] = "test";
    
    ps::pq::Navigator nav;
    ps::pq::PathParser parser;
    auto tokens = parser.parse("name");
    
    auto result = nav.navigate(d, tokens);
    REQUIRE(result.asString() == "test");
}

TEST_CASE("Navigate to deeply nested value", "[pq][navigator][unit]") {
    ps::Dictionary d;
    d["a"]["b"]["c"]["d"] = 42;
    
    ps::pq::Navigator nav;
    ps::pq::PathParser parser;
    auto tokens = parser.parse("a/b/c/d");
    
    auto result = nav.navigate(d, tokens);
    REQUIRE(result.asInt() == 42);
}

TEST_CASE("Navigate to value with spaces in keys", "[pq][navigator][unit]") {
    ps::Dictionary d;
    d["server config"]["port number"] = 9000;
    
    ps::pq::Navigator nav;
    ps::pq::PathParser parser;
    auto tokens = parser.parse("server config/port number");
    
    auto result = nav.navigate(d, tokens);
    REQUIRE(result.asInt() == 9000);
}

TEST_CASE("Navigate to missing key throws exception", "[pq][navigator][unit][exception]") {
    ps::Dictionary d;
    d["existing"] = 1;
    
    ps::pq::Navigator nav;
    ps::pq::PathParser parser;
    auto tokens = parser.parse("missing/key");
    
    REQUIRE_THROWS_AS(nav.navigate(d, tokens), std::out_of_range);
}

TEST_CASE("Navigate to different value types", "[pq][navigator][unit]") {
    ps::Dictionary d;
    d["int_val"] = 42;
    d["double_val"] = 3.14;
    d["string_val"] = "hello";
    d["bool_val"] = true;
    
    ps::pq::Navigator nav;
    ps::pq::PathParser parser;
    
    REQUIRE(nav.navigate(d, parser.parse("int_val")).asInt() == 42);
    REQUIRE(nav.navigate(d, parser.parse("double_val")).asDouble() == 3.14);
    REQUIRE(nav.navigate(d, parser.parse("string_val")).asString() == "hello");
    REQUIRE(nav.navigate(d, parser.parse("bool_val")).asBool() == true);
}

TEST_CASE("Navigate returns object if path ends at object", "[pq][navigator][unit]") {
    ps::Dictionary d;
    d["server"]["host"] = "localhost";
    d["server"]["port"] = 8080;
    
    ps::pq::Navigator nav;
    ps::pq::PathParser parser;
    auto tokens = parser.parse("server");
    
    auto result = nav.navigate(d, tokens);
    REQUIRE(result.has("host"));
    REQUIRE(result.has("port"));
    REQUIRE(result.at("host").asString() == "localhost");
}

// Phase 2.2 - Navigate Array Indices

TEST_CASE("Navigate to array element by index", "[pq][navigator][unit]") {
    ps::Dictionary d;
    d["users"][0] = "Alice";
    d["users"][1] = "Bob";
    d["users"][2] = "Charlie";
    
    ps::pq::Navigator nav;
    ps::pq::PathParser parser;
    
    REQUIRE(nav.navigate(d, parser.parse("users/0")).asString() == "Alice");
    REQUIRE(nav.navigate(d, parser.parse("users/1")).asString() == "Bob");
    REQUIRE(nav.navigate(d, parser.parse("users/2")).asString() == "Charlie");
}

TEST_CASE("Navigate to nested value in array element", "[pq][navigator][unit]") {
    ps::Dictionary d;
    d["users"][0]["name"] = "Alice";
    d["users"][0]["age"] = 30;
    d["users"][1]["name"] = "Bob";
    d["users"][1]["age"] = 25;
    
    ps::pq::Navigator nav;
    ps::pq::PathParser parser;
    
    REQUIRE(nav.navigate(d, parser.parse("users/0/name")).asString() == "Alice");
    REQUIRE(nav.navigate(d, parser.parse("users/1/age")).asInt() == 25);
}

TEST_CASE("Navigate array index out of bounds throws", "[pq][navigator][unit][exception]") {
    ps::Dictionary d;
    d["arr"][0] = 1;
    d["arr"][1] = 2;
    
    ps::pq::Navigator nav;
    ps::pq::PathParser parser;
    
    REQUIRE_THROWS_AS(nav.navigate(d, parser.parse("arr/5")), std::out_of_range);
    REQUIRE_THROWS_AS(nav.navigate(d, parser.parse("arr/100")), std::out_of_range);
}

TEST_CASE("Navigate to deeply nested array structures", "[pq][navigator][unit]") {
    ps::Dictionary d;
    d["data"][0][1]["value"] = 42;
    
    ps::pq::Navigator nav;
    ps::pq::PathParser parser;
    
    REQUIRE(nav.navigate(d, parser.parse("data/0/1/value")).asInt() == 42);
}

// Phase 2.3 - Navigate Wildcards

TEST_CASE("Navigate wildcard returns all array elements", "[pq][navigator][unit]") {
    ps::Dictionary d;
    d["users"][0] = "Alice";
    d["users"][1] = "Bob";
    d["users"][2] = "Charlie";
    
    ps::pq::Navigator nav;
    ps::pq::PathParser parser;
    auto tokens = parser.parse("users/*");
    
    auto results = nav.navigateWildcard(d, tokens);
    REQUIRE(results.size() == 3);
    REQUIRE(results[0].asString() == "Alice");
    REQUIRE(results[1].asString() == "Bob");
    REQUIRE(results[2].asString() == "Charlie");
}

TEST_CASE("Navigate wildcard with nested field", "[pq][navigator][unit]") {
    ps::Dictionary d;
    d["users"][0]["name"] = "Alice";
    d["users"][1]["name"] = "Bob";
    d["users"][2]["name"] = "Charlie";
    
    ps::pq::Navigator nav;
    ps::pq::PathParser parser;
    auto tokens = parser.parse("users/*/name");
    
    auto results = nav.navigateWildcard(d, tokens);
    REQUIRE(results.size() == 3);
    REQUIRE(results[0].asString() == "Alice");
    REQUIRE(results[1].asString() == "Bob");
    REQUIRE(results[2].asString() == "Charlie");
}

TEST_CASE("Navigate multiple wildcards", "[pq][navigator][unit]") {
    ps::Dictionary d;
    d["regions"][0]["cells"][0] = 100;
    d["regions"][0]["cells"][1] = 101;
    d["regions"][1]["cells"][0] = 200;
    d["regions"][1]["cells"][1] = 201;
    
    ps::pq::Navigator nav;
    ps::pq::PathParser parser;
    auto tokens = parser.parse("regions/*/cells/*");
    
    auto results = nav.navigateWildcard(d, tokens);
    REQUIRE(results.size() == 4);
    REQUIRE(results[0].asInt() == 100);
    REQUIRE(results[1].asInt() == 101);
    REQUIRE(results[2].asInt() == 200);
    REQUIRE(results[3].asInt() == 201);
}

TEST_CASE("Navigate wildcard on empty array returns empty", "[pq][navigator][unit]") {
    ps::Dictionary d;
    d["empty"] = ps::Dictionary(); // Empty array
    
    ps::pq::Navigator nav;
    ps::pq::PathParser parser;
    auto tokens = parser.parse("empty/*");
    
    auto results = nav.navigateWildcard(d, tokens);
    REQUIRE(results.size() == 0);
}

TEST_CASE("Navigate without wildcard uses navigate()", "[pq][navigator][unit]") {
    ps::Dictionary d;
    d["server"]["port"] = 8080;
    
    ps::pq::Navigator nav;
    ps::pq::PathParser parser;
    auto tokens = parser.parse("server/port");
    
    auto results = nav.navigateWildcard(d, tokens);
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].asInt() == 8080);
}
