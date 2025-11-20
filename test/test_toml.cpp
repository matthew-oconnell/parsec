#include <catch2/catch_test_macros.hpp>
#include <ps/toml.h>
#include <string>

TEST_CASE("Parse simple TOML key-value pairs", "[toml]") {
    std::string toml = R"(
title = "TOML Example"
number = 42
pi = 3.14159
enabled = true
disabled = false
)";
    auto d = ps::parse_toml(toml);
    REQUIRE(d.at("title").asString() == "TOML Example");
    REQUIRE(d.at("number").asInt() == 42);
    REQUIRE(d.at("pi").asDouble() == 3.14159);
    REQUIRE(d.at("enabled").asBool() == true);
    REQUIRE(d.at("disabled").asBool() == false);
}

TEST_CASE("Parse TOML arrays", "[toml]") {
    std::string toml = R"(
integers = [1, 2, 3, 4, 5]
strings = ["red", "yellow", "green"]
floats = [1.1, 2.2, 3.3]
mixed = [1, "two", 3.0]
)";
    auto d = ps::parse_toml(toml);
    
    auto ints = d.at("integers").asInts();
    REQUIRE(ints.size() == 5);
    REQUIRE(ints[0] == 1);
    REQUIRE(ints[4] == 5);
    
    auto strs = d.at("strings").asStrings();
    REQUIRE(strs.size() == 3);
    REQUIRE(strs[0] == "red");
    REQUIRE(strs[2] == "green");
    
    auto flts = d.at("floats").asDoubles();
    REQUIRE(flts.size() == 3);
    REQUIRE(flts[0] == 1.1);
    REQUIRE(flts[2] == 3.3);
}

TEST_CASE("Parse TOML inline tables", "[toml]") {
    std::string toml = R"(
point = { x = 1, y = 2 }
color = { r = 255, g = 128, b = 0 }
)";
    auto d = ps::parse_toml(toml);
    
    REQUIRE(d.at("point").at("x").asInt() == 1);
    REQUIRE(d.at("point").at("y").asInt() == 2);
    
    REQUIRE(d.at("color").at("r").asInt() == 255);
    REQUIRE(d.at("color").at("g").asInt() == 128);
    REQUIRE(d.at("color").at("b").asInt() == 0);
}

TEST_CASE("Parse TOML tables", "[toml]") {
    std::string toml = R"(
[server]
host = "localhost"
port = 8080
enabled = true

[database]
host = "db.example.com"
port = 5432
)";
    auto d = ps::parse_toml(toml);
    
    REQUIRE(d.at("server").at("host").asString() == "localhost");
    REQUIRE(d.at("server").at("port").asInt() == 8080);
    REQUIRE(d.at("server").at("enabled").asBool() == true);
    
    REQUIRE(d.at("database").at("host").asString() == "db.example.com");
    REQUIRE(d.at("database").at("port").asInt() == 5432);
}

TEST_CASE("Parse TOML nested tables", "[toml]") {
    std::string toml = R"(
[server.connection]
timeout = 30
retry = 3

[server.logging]
level = "info"
file = "/var/log/app.log"
)";
    auto d = ps::parse_toml(toml);
    
    REQUIRE(d.at("server").at("connection").at("timeout").asInt() == 30);
    REQUIRE(d.at("server").at("connection").at("retry").asInt() == 3);
    REQUIRE(d.at("server").at("logging").at("level").asString() == "info");
    REQUIRE(d.at("server").at("logging").at("file").asString() == "/var/log/app.log");
}

TEST_CASE("Parse TOML with comments", "[toml]") {
    std::string toml = R"(
# This is a comment
name = "test" # inline comment
# Another comment
value = 42
)";
    auto d = ps::parse_toml(toml);
    REQUIRE(d.at("name").asString() == "test");
    REQUIRE(d.at("value").asInt() == 42);
}

TEST_CASE("Parse TOML strings with escapes", "[toml]") {
    std::string toml = R"(
str1 = "Hello\nWorld"
str2 = "Tab\there"
str3 = "Quote: \"test\""
)";
    auto d = ps::parse_toml(toml);
    REQUIRE(d.at("str1").asString() == "Hello\nWorld");
    REQUIRE(d.at("str2").asString() == "Tab\there");
    REQUIRE(d.at("str3").asString() == "Quote: \"test\"");
}

TEST_CASE("Parse TOML literal strings", "[toml]") {
    std::string toml = R"(
path = 'C:\Users\nodejs\templates'
regex = '<\i\c*\s*>'
)";
    auto d = ps::parse_toml(toml);
    REQUIRE(d.at("path").asString() == "C:\\Users\\nodejs\\templates");
    REQUIRE(d.at("regex").asString() == "<\\i\\c*\\s*>");
}

TEST_CASE("Parse TOML multiline strings", "[toml]") {
    std::string toml = R"(
description = """
This is a
multiline
string"""
)";
    auto d = ps::parse_toml(toml);
    std::string expected = "This is a\nmultiline\nstring";
    REQUIRE(d.at("description").asString() == expected);
}

TEST_CASE("Parse TOML numbers with underscores", "[toml]") {
    std::string toml = R"(
large = 1_000_000
pi = 3.141_592_653
)";
    auto d = ps::parse_toml(toml);
    REQUIRE(d.at("large").asInt() == 1000000);
    REQUIRE(d.at("pi").asDouble() == 3.141592653);
}

TEST_CASE("Parse TOML special float values", "[toml]") {
    std::string toml = R"(
pos_inf = inf
neg_inf = -inf
not_a_num = nan
)";
    auto d = ps::parse_toml(toml);
    REQUIRE(std::isinf(d.at("pos_inf").asDouble()));
    REQUIRE(d.at("pos_inf").asDouble() > 0);
    REQUIRE(std::isinf(d.at("neg_inf").asDouble()));
    REQUIRE(d.at("neg_inf").asDouble() < 0);
    REQUIRE(std::isnan(d.at("not_a_num").asDouble()));
}

TEST_CASE("Parse TOML bare keys", "[toml]") {
    std::string toml = R"(
key = "value"
bare_key = "test"
bare-key = "dashes"
bare_1_2_3 = "numbers"
)";
    auto d = ps::parse_toml(toml);
    REQUIRE(d.at("key").asString() == "value");
    REQUIRE(d.at("bare_key").asString() == "test");
    REQUIRE(d.at("bare-key").asString() == "dashes");
    REQUIRE(d.at("bare_1_2_3").asString() == "numbers");
}

TEST_CASE("Parse TOML quoted keys", "[toml]") {
    std::string toml = R"(
"key with spaces" = "value"
"127.0.0.1" = "localhost"
)";
    auto d = ps::parse_toml(toml);
    REQUIRE(d.at("key with spaces").asString() == "value");
    REQUIRE(d.at("127.0.0.1").asString() == "localhost");
}

TEST_CASE("Parse TOML empty arrays", "[toml]") {
    std::string toml = R"(
empty = []
)";
    auto d = ps::parse_toml(toml);
    REQUIRE(d.at("empty").size() == 0);
}

TEST_CASE("Parse TOML nested arrays", "[toml]") {
    std::string toml = R"(
nested = [[1, 2], [3, 4]]
)";
    auto d = ps::parse_toml(toml);
    auto arr = d.at("nested").asObjects();
    REQUIRE(arr.size() == 2);
    REQUIRE(arr[0].asInts()[0] == 1);
    REQUIRE(arr[0].asInts()[1] == 2);
    REQUIRE(arr[1].asInts()[0] == 3);
    REQUIRE(arr[1].asInts()[1] == 4);
}

TEST_CASE("Parse TOML mixed root and tables", "[toml]") {
    std::string toml = R"(
root_key = "root value"

[section]
nested_key = "nested value"
)";
    auto d = ps::parse_toml(toml);
    REQUIRE(d.at("root_key").asString() == "root value");
    REQUIRE(d.at("section").at("nested_key").asString() == "nested value");
}

TEST_CASE("Parse TOML datetime values", "[toml]") {
    std::string toml = R"(
date = 2024-11-20
datetime = 2024-11-20T10:30:00Z
)";
    auto d = ps::parse_toml(toml);
    // Datetimes are stored as strings for now
    REQUIRE(d.has("date"));
    REQUIRE(d.has("datetime"));
}

TEST_CASE("Parse TOML with trailing commas in arrays", "[toml]") {
    std::string toml = R"(
arr = [1, 2, 3,]
)";
    auto d = ps::parse_toml(toml);
    auto arr = d.at("arr").asInts();
    REQUIRE(arr.size() == 3);
    REQUIRE(arr[2] == 3);
}

TEST_CASE("Parse TOML complex example", "[toml]") {
    std::string toml = R"(
# Application configuration
title = "Configuration Example"
version = "1.0.0"

[owner]
name = "John Doe"
email = "john@example.com"

[database]
server = "192.168.1.1"
ports = [8001, 8001, 8002]
connection_max = 5000
enabled = true

[servers.alpha]
ip = "10.0.0.1"
dc = "eqdc10"

[servers.beta]
ip = "10.0.0.2"
dc = "eqdc10"
)";
    auto d = ps::parse_toml(toml);
    
    REQUIRE(d.at("title").asString() == "Configuration Example");
    REQUIRE(d.at("version").asString() == "1.0.0");
    REQUIRE(d.at("owner").at("name").asString() == "John Doe");
    REQUIRE(d.at("owner").at("email").asString() == "john@example.com");
    REQUIRE(d.at("database").at("server").asString() == "192.168.1.1");
    REQUIRE(d.at("database").at("connection_max").asInt() == 5000);
    REQUIRE(d.at("database").at("enabled").asBool() == true);
    
    auto ports = d.at("database").at("ports").asInts();
    REQUIRE(ports.size() == 3);
    REQUIRE(ports[2] == 8002);
    
    REQUIRE(d.at("servers").at("alpha").at("ip").asString() == "10.0.0.1");
    REQUIRE(d.at("servers").at("beta").at("ip").asString() == "10.0.0.2");
}

TEST_CASE("TOML parse error includes line and column", "[toml]") {
    std::string toml = R"(
invalid syntax here
)";
    try {
        ps::parse_toml(toml);
        FAIL("Expected parse to throw");
    } catch (const std::exception& e) {
        std::string msg = e.what();
        REQUIRE(msg.find("TOML parse error") != std::string::npos);
        REQUIRE(msg.find("line") != std::string::npos);
        REQUIRE(msg.find("column") != std::string::npos);
    }
}

TEST_CASE("TOML unterminated string error", "[toml]") {
    std::string toml = R"(
name = "unterminated
)";
    try {
        ps::parse_toml(toml);
        FAIL("Expected parse to throw");
    } catch (const std::exception& e) {
        std::string msg = e.what();
        REQUIRE(msg.find("unterminated string") != std::string::npos);
    }
}

TEST_CASE("Parse TOML signed numbers", "[toml]") {
    std::string toml = R"(
positive = +42
negative = -42
pos_float = +3.14
neg_float = -3.14
)";
    auto d = ps::parse_toml(toml);
    REQUIRE(d.at("positive").asInt() == 42);
    REQUIRE(d.at("negative").asInt() == -42);
    REQUIRE(d.at("pos_float").asDouble() == 3.14);
    REQUIRE(d.at("neg_float").asDouble() == -3.14);
}

TEST_CASE("Parse TOML boolean arrays", "[toml]") {
    std::string toml = R"(
flags = [true, false, true]
)";
    auto d = ps::parse_toml(toml);
    auto bools = d.at("flags").asBools();
    REQUIRE(bools.size() == 3);
    REQUIRE(bools[0] == true);
    REQUIRE(bools[1] == false);
    REQUIRE(bools[2] == true);
}
