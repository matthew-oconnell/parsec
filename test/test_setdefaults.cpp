#include <catch2/catch_all.hpp>
#include "ps/parsec.hpp"
#include "ps/validate.hpp"

using namespace ps;

TEST_CASE("setDefaults: basic property default", "[defaults]") {
    Dictionary schema; schema["type"] = Value("object");
    Dictionary props; props["port"] = Value(Dictionary{{"type", Value(std::string("integer"))},{"default", Value(int64_t(8080))}});
    schema["properties"] = Value(props);

    Dictionary input; // empty
    auto out = setDefaults(input, schema);
    REQUIRE(out.has("port"));
    REQUIRE(out.at("port").isInt());
    REQUIRE(out.at("port").asInt() == 8080);
}

TEST_CASE("setDefaults: nested object defaults", "[defaults]") {
    Dictionary innerSchema; innerSchema["type"] = Value("object");
    Dictionary innerProps; innerProps["x"] = Value(Dictionary{{"type", Value(std::string("integer"))},{"default", Value(int64_t(7))}});
    innerSchema["properties"] = Value(innerProps);

    Dictionary schema; schema["type"] = Value("object");
    Dictionary props; props["child"] = Value(innerSchema);
    schema["properties"] = Value(props);

    Dictionary input; // child missing
    auto out = setDefaults(input, schema);
    REQUIRE(out.has("child"));
    REQUIRE(out.at("child").isDict());
    REQUIRE(out.at("child").asDict()->has("x"));
    REQUIRE(out.at("child").asDict()->at("x").asInt() == 7);
}

TEST_CASE("setDefaults: additionalProperties schema applied to existing extras", "[defaults]") {
    Dictionary schema; schema["type"] = Value("object");
    Dictionary add; add["type"] = Value("object");
    Dictionary addProps; addProps["y"] = Value(Dictionary{{"type", Value(std::string("integer"))},{"default", Value(int64_t(2))}});
    add["properties"] = Value(addProps);
    schema["additionalProperties"] = Value(add);

    Dictionary input; input["extra"] = Value(Dictionary{{"z", Value(1)}});
    auto out = setDefaults(input, schema);
    REQUIRE(out.has("extra"));
    auto extra = out.at("extra").asDict();
    REQUIRE(extra->has("y"));
    REQUIRE(extra->at("y").asInt() == 2);
}
