#include <catch2/catch_all.hpp>
#include <ps/ron.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("parse simple RON example") {
#ifndef EXAMPLES_DIR
    FAIL("EXAMPLES_DIR not defined");
#endif
    fs::path examples = EXAMPLES_DIR;
    auto p = examples / "sample.ron";
    REQUIRE(fs::exists(p));
    std::ifstream in(p);
    REQUIRE(in.good());
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto v = ps::parse_ron(content);
    REQUIRE(v.has("visualization"));
    REQUIRE(v.at("visualization").at(0).has("type"));
    REQUIRE(v.at("visualization").at(0).at("type").isString());
    REQUIRE(v.at("visualization").at(0).at("type").asString() == "volume");
}

TEST_CASE("parse RON for empty object") {
    std::string ron_text = "{}";
    auto v = ps::parse_ron(ron_text);
    REQUIRE(v.isDict());
    REQUIRE(v.size() == 0);
}

TEST_CASE("Duplicate keys throw an exception for ron parsing") {
    std::string s = R"(
{
  key1: "value1"
  key2: "value2"
  key1: "value3"
}
)";
    try {
        ps::parse_ron(s);
        FAIL("expected parse to throw");
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        REQUIRE((msg.find("duplicate key") != std::string::npos));
    }
}

TEST_CASE("Extra trailing closing brace is not an error ron ") {
    std::string s = R"(
{
  key1: "value1"
  key2: "value2"
}}
)";
    auto dict = ps::parse_ron(s);
    REQUIRE(dict.size() == 2);
}

TEST_CASE("RON parser handles scientific notation with explicit plus sign") {
    std::string s = R"(
    {
        a: 1e5,
        b: 1e+5,
        c: 1e-5,
        d: 2.5e+10,
        e: 3.14e-20,
        f: 1.23e100
    }
    )";
    auto v = ps::parse_ron(s);
    REQUIRE(v.has("a"));
    REQUIRE(v.at("a").asDouble() == 1e5);
    REQUIRE(v.has("b"));
    REQUIRE(v.at("b").asDouble() == 1e+5);
    REQUIRE(v.has("c"));
    REQUIRE(v.at("c").asDouble() == 1e-5);
    REQUIRE(v.has("d"));
    REQUIRE(v.at("d").asDouble() == 2.5e+10);
    REQUIRE(v.has("e"));
    REQUIRE(v.at("e").asDouble() == 3.14e-20);
    REQUIRE(v.has("f"));
    REQUIRE(v.at("f").asDouble() == 1.23e100);
}

TEST_CASE("RON parser does not confuse identifiers containing 'e' with scientific notation") {
    std::string s = R"(
    {
        type: volume,
        name: envelope,
        mode: reverse,
        device: hardware
    }
    )";
    auto v = ps::parse_ron(s);
    REQUIRE(v.has("type"));
    REQUIRE(v.at("type").isString());
    REQUIRE(v.at("type").asString() == "volume");
    REQUIRE(v.has("name"));
    REQUIRE(v.at("name").isString());
    REQUIRE(v.at("name").asString() == "envelope");
    REQUIRE(v.has("mode"));
    REQUIRE(v.at("mode").isString());
    REQUIRE(v.at("mode").asString() == "reverse");
    REQUIRE(v.has("device"));
    REQUIRE(v.at("device").isString());
    REQUIRE(v.at("device").asString() == "hardware");
}

TEST_CASE("RON parser handles quoted keys with special characters") {
    std::string s = R"(
    {
        "key with spaces": 1,
        "key-with-hyphens": 2,
        "key.with.dots": 3,
        "$special": 4,
        "123numeric": 5
    }
    )";
    auto v = ps::parse_ron(s);
    REQUIRE(v.has("key with spaces"));
    REQUIRE(v.at("key with spaces").asInt() == 1);
    REQUIRE(v.has("key-with-hyphens"));
    REQUIRE(v.at("key-with-hyphens").asInt() == 2);
    REQUIRE(v.has("key.with.dots"));
    REQUIRE(v.at("key.with.dots").asInt() == 3);
    REQUIRE(v.has("$special"));
    REQUIRE(v.at("$special").asInt() == 4);
    REQUIRE(v.has("123numeric"));
    REQUIRE(v.at("123numeric").asInt() == 5);
}

TEST_CASE("RON printer quotes keys with special characters") {
    ps::Dictionary d;
    d["key with spaces"] = "value1";
    d["key-with-hyphens"] = "value2";
    d["key.with.dots"] = "value3";
    d["normal_key"] = "value4";
    d["$special"] = "value5";
    
    std::string ron_output = ps::dump_ron(d);
    
    // Keys with spaces, hyphens, and dots should be quoted
    REQUIRE(ron_output.find("\"key with spaces\":") != std::string::npos);
    REQUIRE(ron_output.find("\"key-with-hyphens\":") != std::string::npos);
    REQUIRE(ron_output.find("\"key.with.dots\":") != std::string::npos);
    
    // Normal keys should not be quoted
    REQUIRE(ron_output.find("normal_key:") != std::string::npos);
    
    // Dollar sign keys should not be quoted (allowed in RON)
    REQUIRE(ron_output.find("$special:") != std::string::npos);
    
    // Verify the output can be parsed back
    auto parsed = ps::parse_ron(ron_output);
    REQUIRE(parsed.at("key with spaces").asString() == "value1");
    REQUIRE(parsed.at("key-with-hyphens").asString() == "value2");
    REQUIRE(parsed.at("key.with.dots").asString() == "value3");
    REQUIRE(parsed.at("normal_key").asString() == "value4");
    REQUIRE(parsed.at("$special").asString() == "value5");
}

TEST_CASE("RON round-trip with scientific notation and special keys") {
    ps::Dictionary d;
    d["boundary conditions"] = std::vector<ps::Dictionary>{};
    d["cfl bounds"] = std::vector<double>{0.001, 1e+200};
    d["temperature"] = 3.14159e-10;
    d["count"] = 1000000;
    d["$ref"] = "schema.json";
    
    std::string ron_output = ps::dump_ron(d);
    auto parsed = ps::parse_ron(ron_output);
    
    REQUIRE(parsed.has("boundary conditions"));
    REQUIRE(parsed.has("cfl bounds"));
    REQUIRE(parsed.at("cfl bounds").size() == 2);
    REQUIRE(parsed.at("cfl bounds").at(0).asDouble() == 0.001);
    REQUIRE(parsed.at("cfl bounds").at(1).asDouble() == 1e+200);
    REQUIRE(parsed.has("temperature"));
    REQUIRE(parsed.at("temperature").asDouble() == 3.14159e-10);
    REQUIRE(parsed.has("count"));
    REQUIRE(parsed.at("count").asInt() == 1000000);
    REQUIRE(parsed.has("$ref"));
    REQUIRE(parsed.at("$ref").asString() == "schema.json");
}
