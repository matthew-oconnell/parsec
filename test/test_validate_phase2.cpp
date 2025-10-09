#include <catch2/catch_all.hpp>
#include "ps/parsec.hpp"
#include "ps/validate.hpp"

using namespace ps;

TEST_CASE("validate phase2: oneOf/anyOf/allOf", "[validate][phase2]") {
    Dictionary schema;
    // allOf: must be both integer and >= 5
    Dictionary s1; s1["type"] = Value(std::string("integer"));
    Dictionary s2; s2["minimum"] = Value(int64_t(5));
    Dictionary all; all["allOf"] = Value(Value::list_t{ Value(s1), Value(s2) });

    SECTION("allOf success") {
        Dictionary d; d.scalar = Value(int64_t(6));
        auto e = validate(d, all);
        REQUIRE(!e.has_value());
    }

    SECTION("allOf failure") {
        Dictionary d; d.scalar = Value(int64_t(3));
        auto e = validate(d, all);
        REQUIRE(e.has_value());
    }

    // anyOf: either integer or string
    Dictionary any; any["anyOf"] = Value(Value::list_t{ Value(s1), Value(Dictionary{{"type", Value(std::string("string"))}}) });
    SECTION("anyOf match") { Dictionary d; d.scalar = Value(std::string("hi")); REQUIRE(!validate(d, any).has_value()); }
    SECTION("anyOf no match") { Dictionary d; d.scalar = Value(1.23); REQUIRE(validate(d, any).has_value()); }

    // oneOf: exactly one alternative
    Dictionary o1; o1["type"] = Value(std::string("integer")); o1["minimum"] = Value(int64_t(0));
    Dictionary o2; o2["type"] = Value(std::string("integer")); o2["maximum"] = Value(int64_t(10));
    Dictionary one; one["oneOf"] = Value(Value::list_t{ Value(o1), Value(o2) });
    SECTION("oneOf ambiguous") { Dictionary d; d.scalar = Value(int64_t(5)); REQUIRE(validate(d, one).has_value()); }
}

TEST_CASE("validate phase2: tuple items and additionalItems", "[validate][phase2]") {
    Dictionary schema; schema["type"] = Value(std::string("array"));
    // tuple: [int, string], additionalItems=false
    Value::list_t tupleSchemas; tupleSchemas.emplace_back(Dictionary{{"type", Value(std::string("integer"))}});
    tupleSchemas.emplace_back(Dictionary{{"type", Value(std::string("string"))}});
    schema["items"] = Value(tupleSchemas);
    schema["additionalItems"] = Value(false);

    SECTION("tuple valid") { Value::list_t L; L.emplace_back(int64_t(1)); L.emplace_back(std::string("ok")); Dictionary d; d.scalar = Value(L); auto e = validate(d, schema); REQUIRE(!e.has_value()); }
    SECTION("tuple extra item") { Value::list_t L; L.emplace_back(int64_t(1)); L.emplace_back(std::string("ok")); L.emplace_back(int64_t(3)); Dictionary d; d.scalar = Value(L); auto e = validate(d, schema); REQUIRE(e.has_value()); }
}

TEST_CASE("validate phase2: uniqueItems and patternProperties", "[validate][phase2]") {
    Dictionary obj; obj["type"] = Value(std::string("object"));
    Dictionary props; props["id"] = Value(Dictionary{{"type", Value(std::string("integer"))}});
    obj["properties"] = Value(props);
    // patternProperties: keys starting with foo- must be strings
    Dictionary pat; pat["^foo-.*"] = Value(Dictionary{{"type", Value(std::string("string"))}});
    obj["patternProperties"] = Value(pat);

    SECTION("patternProperties match") {
        Dictionary cfg; cfg["foo-bar"] = Value(std::string("yes")); cfg["id"] = Value(int64_t(1)); auto e = validate(cfg, obj); REQUIRE(!e.has_value()); }

    SECTION("patternProperties mismatch") {
        Dictionary cfg; cfg["foo-baz"] = Value(int64_t(5)); auto e = validate(cfg, obj); REQUIRE(e.has_value()); }

    // uniqueItems
    Dictionary arr; arr["type"] = Value(std::string("array")); arr["uniqueItems"] = Value(true);
    Value::list_t L; L.emplace_back(int64_t(1)); L.emplace_back(int64_t(2)); L.emplace_back(int64_t(1)); arr["items"] = Value(Dictionary{{"type", Value(std::string("integer"))}});
    SECTION("uniqueItems violation") { Dictionary d; d.scalar = Value(L); auto e = validate(d, arr); REQUIRE(e.has_value()); }
}
