#include <catch2/catch_all.hpp>
#include "ps/parsec.h"

using namespace ps;

TEST_CASE("Dictionary initializer-list construction", "[init]") {
    Dictionary dict = {
        {"key", "value"},
        {"key2", {"my", "array", "is", "cool"}},
        {"key3", {{"obj key", true}}}
    };

    REQUIRE(dict.has("key"));
    REQUIRE(dict.at("key").asString() == "value");

    REQUIRE(dict.has("key2"));
    auto arr = dict.at("key2");
    REQUIRE(arr.size() == 4);
    REQUIRE(arr[0].asString() == "my");

    REQUIRE(dict.has("key3"));
    auto child = dict.at("key3");
    REQUIRE(child.has("obj key"));
    REQUIRE(child.at("obj key").asBool() == true);
}
