#include <catch2/catch_test_macros.hpp>
#include <ps/ini.h>
#include <string>

TEST_CASE("Parse simple INI key-value pairs", "[ini]") {
    std::string ini = R"(
key1 = value1
key2 = value2
number = 42
)";
    auto d = ps::parse_ini(ini);
    REQUIRE(d.at("key1").asString() == "value1");
    REQUIRE(d.at("key2").asString() == "value2");
    REQUIRE(d.at("number").asInt() == 42);
}

TEST_CASE("Parse INI sections", "[ini]") {
    std::string ini = R"(
[section1]
key1 = value1
key2 = 123

[section2]
key3 = value3
key4 = 456
)";
    auto d = ps::parse_ini(ini);
    REQUIRE(d.at("section1").at("key1").asString() == "value1");
    REQUIRE(d.at("section1").at("key2").asInt() == 123);
    REQUIRE(d.at("section2").at("key3").asString() == "value3");
    REQUIRE(d.at("section2").at("key4").asInt() == 456);
}

TEST_CASE("Parse INI with comments", "[ini]") {
    std::string ini = R"(
; This is a comment
key1 = value1
# This is also a comment
key2 = value2  ; inline comment
)";
    auto d = ps::parse_ini(ini);
    REQUIRE(d.at("key1").asString() == "value1");
    REQUIRE(d.at("key2").asString() == "value2");
}

TEST_CASE("Parse INI with colon separator", "[ini]") {
    std::string ini = R"(
key1: value1
key2: value2
)";
    auto d = ps::parse_ini(ini);
    REQUIRE(d.at("key1").asString() == "value1");
    REQUIRE(d.at("key2").asString() == "value2");
}

TEST_CASE("Parse INI boolean values", "[ini]") {
    std::string ini = R"(
bool1 = true
bool2 = false
bool3 = yes
bool4 = no
bool5 = on
bool6 = off
)";
    auto d = ps::parse_ini(ini);
    REQUIRE(d.at("bool1").asBool() == true);
    REQUIRE(d.at("bool2").asBool() == false);
    REQUIRE(d.at("bool3").asBool() == true);
    REQUIRE(d.at("bool4").asBool() == false);
    REQUIRE(d.at("bool5").asBool() == true);
    REQUIRE(d.at("bool6").asBool() == false);
}

TEST_CASE("Parse INI numeric values", "[ini]") {
    std::string ini = R"(
int_pos = 42
int_neg = -42
float_val = 3.14159
float_neg = -2.718
)";
    auto d = ps::parse_ini(ini);
    REQUIRE(d.at("int_pos").asInt() == 42);
    REQUIRE(d.at("int_neg").asInt() == -42);
    REQUIRE(d.at("float_val").asDouble() == 3.14159);
    REQUIRE(d.at("float_neg").asDouble() == -2.718);
}

TEST_CASE("Parse INI quoted strings", "[ini]") {
    std::string ini = R"(
quoted1 = "value with spaces"
quoted2 = 'another value'
)";
    auto d = ps::parse_ini(ini);
    REQUIRE(d.at("quoted1").asString() == "value with spaces");
    REQUIRE(d.at("quoted2").asString() == "another value");
}

TEST_CASE("Parse INI dotted section names", "[ini]") {
    std::string ini = R"(
[section.subsection]
key1 = value1

[section.subsection.deep]
key2 = value2
)";
    auto d = ps::parse_ini(ini);
    REQUIRE(d.at("section").at("subsection").at("key1").asString() == "value1");
    REQUIRE(d.at("section").at("subsection").at("deep").at("key2").asString() == "value2");
}

TEST_CASE("Parse INI with whitespace trimming", "[ini]") {
    std::string ini = R"(
  key1  =  value1  
  key2=value2
key3   =   value3   
)";
    auto d = ps::parse_ini(ini);
    REQUIRE(d.at("key1").asString() == "value1");
    REQUIRE(d.at("key2").asString() == "value2");
    REQUIRE(d.at("key3").asString() == "value3");
}

TEST_CASE("Parse INI empty values", "[ini]") {
    std::string ini = R"(
empty1 =
empty2 = 
)";
    auto d = ps::parse_ini(ini);
    REQUIRE(d.at("empty1").asString() == "");
    REQUIRE(d.at("empty2").asString() == "");
}

TEST_CASE("Parse INI with empty lines", "[ini]") {
    std::string ini = R"(
key1 = value1

key2 = value2


[section]

key3 = value3
)";
    auto d = ps::parse_ini(ini);
    REQUIRE(d.at("key1").asString() == "value1");
    REQUIRE(d.at("key2").asString() == "value2");
    REQUIRE(d.at("section").at("key3").asString() == "value3");
}

TEST_CASE("Parse INI complex example", "[ini]") {
    std::string ini = R"(
; Application configuration
app_name = MyApp
version = 1.0.0

[server]
host = localhost
port = 8080
enabled = true
timeout = 30

[server.ssl]
enabled = false
port = 443

[database]
host = db.example.com
port = 5432
username = admin
password = secret123

[features]
caching = on
compression = yes
debug = off
)";
    auto d = ps::parse_ini(ini);
    
    REQUIRE(d.at("app_name").asString() == "MyApp");
    REQUIRE(d.at("version").asString() == "1.0.0");
    
    REQUIRE(d.at("server").at("host").asString() == "localhost");
    REQUIRE(d.at("server").at("port").asInt() == 8080);
    REQUIRE(d.at("server").at("enabled").asBool() == true);
    REQUIRE(d.at("server").at("timeout").asInt() == 30);
    
    REQUIRE(d.at("server").at("ssl").at("enabled").asBool() == false);
    REQUIRE(d.at("server").at("ssl").at("port").asInt() == 443);
    
    REQUIRE(d.at("database").at("host").asString() == "db.example.com");
    REQUIRE(d.at("database").at("port").asInt() == 5432);
    
    REQUIRE(d.at("features").at("caching").asBool() == true);
    REQUIRE(d.at("features").at("compression").asBool() == true);
    REQUIRE(d.at("features").at("debug").asBool() == false);
}

TEST_CASE("INI parse error includes line and column", "[ini]") {
    std::string ini = R"(
[unclosed section
)";
    try {
        ps::parse_ini(ini);
        FAIL("Expected parse to throw");
    } catch (const std::exception& e) {
        std::string msg = e.what();
        REQUIRE(msg.find("INI parse error") != std::string::npos);
        REQUIRE(msg.find("line") != std::string::npos);
        REQUIRE(msg.find("column") != std::string::npos);
    }
}

TEST_CASE("INI missing separator error", "[ini]") {
    std::string ini = R"(
key_without_value
)";
    try {
        ps::parse_ini(ini);
        FAIL("Expected parse to throw");
    } catch (const std::exception& e) {
        std::string msg = e.what();
        REQUIRE(msg.find("expected '=' or ':'") != std::string::npos);
    }
}

TEST_CASE("Parse INI mixed case booleans", "[ini]") {
    std::string ini = R"(
bool1 = True
bool2 = FALSE
bool3 = Yes
bool4 = NO
)";
    auto d = ps::parse_ini(ini);
    REQUIRE(d.at("bool1").asBool() == true);
    REQUIRE(d.at("bool2").asBool() == false);
    REQUIRE(d.at("bool3").asBool() == true);
    REQUIRE(d.at("bool4").asBool() == false);
}

TEST_CASE("Parse INI values with special characters", "[ini]") {
    std::string ini = R"(
url = http://example.com/path?query=value
email = user@example.com
path = /usr/local/bin
)";
    auto d = ps::parse_ini(ini);
    REQUIRE(d.at("url").asString() == "http://example.com/path?query=value");
    REQUIRE(d.at("email").asString() == "user@example.com");
    REQUIRE(d.at("path").asString() == "/usr/local/bin");
}

TEST_CASE("Parse INI section with whitespace in name", "[ini]") {
    std::string ini = R"(
[section name]
key = value
)";
    auto d = ps::parse_ini(ini);
    REQUIRE(d.at("section name").at("key").asString() == "value");
}

TEST_CASE("INI keys must start with letter - valid", "[ini]") {
    std::string ini = R"(
alpha = one
AlphaKey = two
a1 = three
)";
    auto d = ps::parse_ini(ini);
    REQUIRE(d.at("alpha").asString() == "one");
    REQUIRE(d.at("AlphaKey").asString() == "two");
    REQUIRE(d.at("a1").asString() == "three");
}

TEST_CASE("INI keys must start with letter - invalid starts", "[ini][error]") {
    std::string ini_digit = R"(
1key = value
)";
    REQUIRE_THROWS(ps::parse_ini(ini_digit));

    std::string ini_underscore = R"(
_key = value
)";
    REQUIRE_THROWS(ps::parse_ini(ini_underscore));
}

TEST_CASE("Parse INI global section and named sections", "[ini]") {
    std::string ini = R"(
global_key = global_value

[section1]
key1 = value1

[section2]
key2 = value2
)";
    auto d = ps::parse_ini(ini);
    REQUIRE(d.at("global_key").asString() == "global_value");
    REQUIRE(d.at("section1").at("key1").asString() == "value1");
    REQUIRE(d.at("section2").at("key2").asString() == "value2");
}

TEST_CASE("Parse INI scientific notation", "[ini]") {
    std::string ini = R"(
sci1 = 1.23e10
sci2 = 4.56E-5
)";
    auto d = ps::parse_ini(ini);
    REQUIRE(d.at("sci1").asDouble() == 1.23e10);
    REQUIRE(d.at("sci2").asDouble() == 4.56E-5);
}

TEST_CASE("don't allow ini keys to start with brackets", "[ini][error]") {
    std::string used_brackets_for_array = R"({"some_array":{{"type":"dog"}, "type":"pokemon"}})";
    REQUIRE_THROWS( ps::parse_ini(used_brackets_for_array));
}