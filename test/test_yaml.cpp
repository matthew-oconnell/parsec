#include <catch2/catch_all.hpp>
#include <ps/yaml.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("parse simple YAML example") {
#ifndef EXAMPLES_DIR
    FAIL("EXAMPLES_DIR not defined");
#endif
    fs::path examples = EXAMPLES_DIR;
    auto p = examples / "sample.yaml";
    REQUIRE(fs::exists(p));
    std::ifstream in(p);
    REQUIRE(in.good());
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto v = ps::parse_yaml(content);
    REQUIRE(v.has("visualization"));
    REQUIRE(v.at("visualization").at(0).has("type"));
    REQUIRE(v.at("visualization").at(0).at("type").isString());
    REQUIRE(v.at("visualization").at(0).at("type").asString() == "volume");
}

TEST_CASE("parse yaml reports if a key is duplicated", "[duplicate_keys]") {
    std::string s = R"(
key1: 1
key2: 2
key1: 3
)";
    try {
        ps::parse_yaml(s);
        FAIL("expected parse to throw");
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        REQUIRE((msg.find("duplicate key") != std::string::npos));
    }
}

TEST_CASE("parse yaml with nested objects") {
    std::string s = R"(
person:
  name: John Doe
  age: 30
  address:
    street: 123 Main St
    city: Springfield
)";
    auto v = ps::parse_yaml(s);
    REQUIRE(v.has("person"));
    REQUIRE(v.at("person").has("name"));
    REQUIRE(v.at("person").at("name").asString() == "John Doe");
    REQUIRE(v.at("person").has("age"));
    REQUIRE(v.at("person").at("age").asInt() == 30);
    REQUIRE(v.at("person").has("address"));
    REQUIRE(v.at("person").at("address").has("street"));
    REQUIRE(v.at("person").at("address").at("street").asString() == "123 Main St");
}

TEST_CASE("parse yaml with arrays") {
    std::string s = R"(
numbers:
  - 1
  - 2
  - 3
  - 4
  - 5
)";
    auto v = ps::parse_yaml(s);
    REQUIRE(v.has("numbers"));
    REQUIRE(v.at("numbers").size() == 5);
    REQUIRE(v.at("numbers").at(0).asInt() == 1);
    REQUIRE(v.at("numbers").at(4).asInt() == 5);
}

TEST_CASE("parse yaml with boolean values") {
    std::string s = R"(
bool1: true
bool2: false
bool3: True
bool4: FALSE
)";
    auto v = ps::parse_yaml(s);
    REQUIRE(v.at("bool1").asBool() == true);
    REQUIRE(v.at("bool2").asBool() == false);
    REQUIRE(v.at("bool3").asBool() == true);
    REQUIRE(v.at("bool4").asBool() == false);
}

TEST_CASE("parse yaml with null values") {
    std::string s = R"(
null1: null
null2: ~
null3: Null
)";
    auto v = ps::parse_yaml(s);
    REQUIRE(v.at("null1").isNull());
    REQUIRE(v.at("null2").isNull());
    REQUIRE(v.at("null3").isNull());
}

TEST_CASE("parse yaml with quoted strings") {
    std::string s = R"(
str1: "hello world"
str2: 'single quoted'
str3: unquoted string
)";
    auto v = ps::parse_yaml(s);
    REQUIRE(v.at("str1").asString() == "hello world");
    REQUIRE(v.at("str2").asString() == "single quoted");
    REQUIRE(v.at("str3").asString() == "unquoted string");
}

TEST_CASE("parse yaml with numbers") {
    std::string s = R"(
int1: 42
int2: -17
float1: 3.14
float2: -2.5
scientific: 1.23e10
)";
    auto v = ps::parse_yaml(s);
    REQUIRE(v.at("int1").asInt() == 42);
    REQUIRE(v.at("int2").asInt() == -17);
    REQUIRE(v.at("float1").asDouble() == 3.14);
    REQUIRE(v.at("float2").asDouble() == -2.5);
    REQUIRE(v.at("scientific").asDouble() == 1.23e10);
}

TEST_CASE("parse yaml with comments") {
    std::string s = R"(
# This is a comment
key1: value1  # inline comment
# Another comment
key2: value2
)";
    auto v = ps::parse_yaml(s);
    REQUIRE(v.has("key1"));
    REQUIRE(v.at("key1").asString() == "value1");
    REQUIRE(v.has("key2"));
    REQUIRE(v.at("key2").asString() == "value2");
}

TEST_CASE("parse yaml with mixed array types") {
    std::string s = R"(
mixed:
  - name: item1
    value: 10
  - name: item2
    value: 20
)";
    auto v = ps::parse_yaml(s);
    REQUIRE(v.has("mixed"));
    REQUIRE(v.at("mixed").size() == 2);
    REQUIRE(v.at("mixed").at(0).has("name"));
    REQUIRE(v.at("mixed").at(0).at("name").asString() == "item1");
    REQUIRE(v.at("mixed").at(1).at("value").asInt() == 20);
}

TEST_CASE("parse empty yaml returns empty object") {
    std::string s = "";
    auto v = ps::parse_yaml(s);
    REQUIRE(v.isMappedObject());
}
