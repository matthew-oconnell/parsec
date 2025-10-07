#include <catch2/catch_all.hpp>
#include <ps/dictionary.hpp>

using namespace ps;

TEST_CASE("Dictionary basic operations") {
    Dictionary d;
    REQUIRE(d.empty());

    d["one"] = 1;
    d["pi"] = 3.1415;
    d["name"] = std::string("parsec");

    REQUIRE(d.size() == 3);
    REQUIRE(d.contains("one"));
    REQUIRE(d.contains("pi"));
    REQUIRE(d.contains("name"));

    REQUIRE(d.at("one").is_int());
    REQUIRE(d.at("pi").is_double());
    REQUIRE(d.at("name").is_string());

    auto keys = d.keys();
    REQUIRE(keys.size() == 3);

    auto items = d.items();
    REQUIRE(items.size() == 3);

    d.erase("one");
    REQUIRE(!d.contains("one"));
    REQUIRE(d.size() == 2);
}
