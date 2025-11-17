#include <catch2/catch_all.hpp>
#include "ps/parsec.h"
#include "ps/validate.h"

using namespace ps;

TEST_CASE("setDefaults: basic property default", "[defaults]") {
    Dictionary schema;
    schema["type"] = "object";
    Dictionary props;
    props["port"] = Dictionary{{"type", "integer"}, {"default", int64_t(8080)}};
    schema["properties"] = props;

    Dictionary input;  // empty
    auto out = setDefaults(input, schema);
    REQUIRE(out.has("port"));
    REQUIRE(out.at("port").isInt());
    REQUIRE(out.at("port").asInt() == 8080);
}

TEST_CASE("setDefaults: nested object defaults", "[defaults]") {
    Dictionary innerSchema;
    innerSchema["type"] = "object";
    Dictionary innerProps;
    innerProps["x"] = Dictionary{{"type", "integer"}, {"default", int64_t(7)}};
    innerSchema["properties"] = innerProps;

    Dictionary schema;
    schema["type"] = "object";
    Dictionary props;
    props["child"] = innerSchema;
    schema["properties"] = props;

    Dictionary input;  // child missing
    auto out = setDefaults(input, schema);
    REQUIRE(out.has("child"));
    REQUIRE(out.at("child").has("x"));
    REQUIRE(out.at("child").at("x").asInt() == 7);
}

TEST_CASE("setDefaults: additionalProperties schema applied to existing extras", "[defaults]") {
    Dictionary schema;
    schema["type"] = "object";
    Dictionary add;
    add["type"] = "object";
    Dictionary addProps;
    addProps["y"] = Dictionary{{"type", "integer"}, {"default", int64_t(2)}};
    add["properties"] = addProps;
    schema["additionalProperties"] = add;
    Dictionary input;
    input["extra"] = Dictionary{{"z", int64_t(1)}};
    auto out = setDefaults(input, schema);
    REQUIRE(out.has("extra"));
    auto extra = out.at("extra");
    REQUIRE(extra.isDict());
    REQUIRE(extra.has("y"));
    REQUIRE(extra.at("y").asInt() == 2);
}
