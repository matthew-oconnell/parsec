#include <catch2/catch_test_macros.hpp>
#include <ps/pq/path_parser.h>
#include <stdexcept>

TEST_CASE("Parse simple two-segment path", "[pq][path_parser][unit]") {
    ps::pq::PathParser parser;
    auto tokens = parser.parse("server/port");
    
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].isKey());
    REQUIRE(tokens[0].asKey() == "server");
    REQUIRE(tokens[1].isKey());
    REQUIRE(tokens[1].asKey() == "port");
}

TEST_CASE("Parse multi-segment path", "[pq][path_parser][unit]") {
    ps::pq::PathParser parser;
    auto tokens = parser.parse("a/b/c/d");
    
    REQUIRE(tokens.size() == 4);
    REQUIRE(tokens[0].asKey() == "a");
    REQUIRE(tokens[1].asKey() == "b");
    REQUIRE(tokens[2].asKey() == "c");
    REQUIRE(tokens[3].asKey() == "d");
}

TEST_CASE("Parse single segment path", "[pq][path_parser][unit]") {
    ps::pq::PathParser parser;
    auto tokens = parser.parse("single");
    
    REQUIRE(tokens.size() == 1);
    REQUIRE(tokens[0].isKey());
    REQUIRE(tokens[0].asKey() == "single");
}

TEST_CASE("Parse empty path throws error", "[pq][path_parser][unit][exception]") {
    ps::pq::PathParser parser;
    
    REQUIRE_THROWS_AS(parser.parse(""), std::invalid_argument);
}

TEST_CASE("Parse path with leading slash throws error", "[pq][path_parser][unit][exception]") {
    ps::pq::PathParser parser;
    
    REQUIRE_THROWS_AS(parser.parse("/leading/slash"), std::invalid_argument);
}

TEST_CASE("Parse path with trailing slash throws error", "[pq][path_parser][unit][exception]") {
    ps::pq::PathParser parser;
    
    REQUIRE_THROWS_AS(parser.parse("trailing/slash/"), std::invalid_argument);
}

TEST_CASE("Parse path with double slash throws error", "[pq][path_parser][unit][exception]") {
    ps::pq::PathParser parser;
    
    REQUIRE_THROWS_AS(parser.parse("double//slash"), std::invalid_argument);
}

TEST_CASE("Parse path with spaces in keys", "[pq][path_parser][unit]") {
    ps::pq::PathParser parser;
    auto tokens = parser.parse("server config/port number");
    
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].asKey() == "server config");
    REQUIRE(tokens[1].asKey() == "port number");
}

// Phase 1.2 - Array Index Tests
TEST_CASE("Parse array index at end of path", "[pq][path_parser][unit]") {
    ps::pq::PathParser parser;
    auto tokens = parser.parse("users/0");
    
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].isKey());
    REQUIRE(tokens[0].asKey() == "users");
    REQUIRE(tokens[1].isIndex());
    REQUIRE(tokens[1].asIndex() == 0);
}

TEST_CASE("Parse array index in middle of path", "[pq][path_parser][unit]") {
    ps::pq::PathParser parser;
    auto tokens = parser.parse("users/0/name");
    
    REQUIRE(tokens.size() == 3);
    REQUIRE(tokens[0].asKey() == "users");
    REQUIRE(tokens[1].isIndex());
    REQUIRE(tokens[1].asIndex() == 0);
    REQUIRE(tokens[2].asKey() == "name");
}

TEST_CASE("Parse multiple array indices", "[pq][path_parser][unit]") {
    ps::pq::PathParser parser;
    auto tokens = parser.parse("items/10/nested/0");
    
    REQUIRE(tokens.size() == 4);
    REQUIRE(tokens[0].asKey() == "items");
    REQUIRE(tokens[1].isIndex());
    REQUIRE(tokens[1].asIndex() == 10);
    REQUIRE(tokens[2].asKey() == "nested");
    REQUIRE(tokens[3].isIndex());
    REQUIRE(tokens[3].asIndex() == 0);
}

TEST_CASE("Parse large array index", "[pq][path_parser][unit]") {
    ps::pq::PathParser parser;
    auto tokens = parser.parse("arr/999");
    
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[1].asIndex() == 999);
}

TEST_CASE("Negative index throws error", "[pq][path_parser][unit][exception]") {
    ps::pq::PathParser parser;
    
    REQUIRE_THROWS_AS(parser.parse("arr/-1"), std::invalid_argument);
}

TEST_CASE("String that looks numeric is treated as key", "[pq][path_parser][unit]") {
    ps::pq::PathParser parser;
    auto tokens = parser.parse("obj/123abc");
    
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[1].isKey());
    REQUIRE(tokens[1].asKey() == "123abc");
}

// Phase 1.3 - Wildcard Tests
TEST_CASE("Parse wildcard at end of path", "[pq][path_parser][unit]") {
    ps::pq::PathParser parser;
    auto tokens = parser.parse("users/*");
    
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].isKey());
    REQUIRE(tokens[0].asKey() == "users");
    REQUIRE(tokens[1].isWildcard());
}

TEST_CASE("Parse wildcard in middle of path", "[pq][path_parser][unit]") {
    ps::pq::PathParser parser;
    auto tokens = parser.parse("users/*/name");
    
    REQUIRE(tokens.size() == 3);
    REQUIRE(tokens[0].asKey() == "users");
    REQUIRE(tokens[1].isWildcard());
    REQUIRE(tokens[2].asKey() == "name");
}

TEST_CASE("Parse multiple wildcards", "[pq][path_parser][unit]") {
    ps::pq::PathParser parser;
    auto tokens = parser.parse("a/*/b/*/c");
    
    REQUIRE(tokens.size() == 5);
    REQUIRE(tokens[0].asKey() == "a");
    REQUIRE(tokens[1].isWildcard());
    REQUIRE(tokens[2].asKey() == "b");
    REQUIRE(tokens[3].isWildcard());
    REQUIRE(tokens[4].asKey() == "c");
}

TEST_CASE("Parse standalone wildcard", "[pq][path_parser][unit]") {
    ps::pq::PathParser parser;
    auto tokens = parser.parse("*");
    
    REQUIRE(tokens.size() == 1);
    REQUIRE(tokens[0].isWildcard());
}

// Dot notation support
TEST_CASE("Parse dot notation path", "[pq][path_parser][unit]") {
    ps::pq::PathParser parser;
    auto tokens = parser.parse("server.port");
    
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].asKey() == "server");
    REQUIRE(tokens[1].asKey() == "port");
}

TEST_CASE("Parse dot notation with array index", "[pq][path_parser][unit]") {
    ps::pq::PathParser parser;
    auto tokens = parser.parse("users.0.name");
    
    REQUIRE(tokens.size() == 3);
    REQUIRE(tokens[0].asKey() == "users");
    REQUIRE(tokens[1].isIndex());
    REQUIRE(tokens[1].asIndex() == 0);
    REQUIRE(tokens[2].asKey() == "name");
}

TEST_CASE("Parse dot notation with wildcard", "[pq][path_parser][unit]") {
    ps::pq::PathParser parser;
    auto tokens = parser.parse("users.*.email");
    
    REQUIRE(tokens.size() == 3);
    REQUIRE(tokens[0].asKey() == "users");
    REQUIRE(tokens[1].isWildcard());
    REQUIRE(tokens[2].asKey() == "email");
}

TEST_CASE("Slash takes precedence over dot", "[pq][path_parser][unit]") {
    ps::pq::PathParser parser;
    // If path has slashes, dots are part of key names
    auto tokens = parser.parse("server.config/port.number");
    
    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].asKey() == "server.config");
    REQUIRE(tokens[1].asKey() == "port.number");
}
