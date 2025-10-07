#include <catch2/catch_all.hpp>
#include <ps/simple_json.hpp>

TEST_CASE("parser supports // line comments and /* block comments */") {
    using namespace ps;

    SECTION("line comment") {
        std::string s = R"(
        // this is a comment
        {"a": 1} // trailing comment
        )";
        auto v = parse_json(s);
        REQUIRE(v.is_dict());
        REQUIRE(v.as_dict()->data.at("a").is_int());
        REQUIRE(v.as_dict()->data.at("a").as_int() == 1);
    }

    SECTION("block comment") {
        std::string s = R"(
        /* start comment
           still comment */
        {"b": 2}
        )";
        auto v = parse_json(s);
        REQUIRE(v.is_dict());
        REQUIRE(v.as_dict()->data.at("b").is_int());
        REQUIRE(v.as_dict()->data.at("b").as_int() == 2);
    }

    SECTION("inline block comment") {
        std::string s = R"({/*c*/"c":3})";
        auto v = parse_json(s);
        REQUIRE(v.is_dict());
        REQUIRE(v.as_dict()->data.at("c").as_int() == 3);
    }
}
