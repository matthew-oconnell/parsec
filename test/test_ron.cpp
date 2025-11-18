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