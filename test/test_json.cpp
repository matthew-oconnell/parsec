#include <catch2/catch_all.hpp>
#include <ps/dictionary.h>
#include <ps/json.h>

using namespace ps;
using Catch::Approx;
using Catch::Matchers::Contains;

TEST_CASE("Prevent precision issues") {
    std::string c =
                R"({"object w/array that get reparsed with int + double members":{"array":[1.0,1.0e+200]}})";
    auto dict = parse_json(c);
    auto reparse = parse_json(dict.dump());
    REQUIRE_NOTHROW(reparse.dump(4));
    INFO(reparse.dump(4));
    const auto& array =
                dict.at("object w/array that get reparsed with int + double members").at("array");
    REQUIRE(array.type() == Dictionary::DoubleArray);
}

TEST_CASE("Can merge two JSON objects into a new JSON object") {
    std::string defaults = R"({
    "nested object1": {
        "prop1": "default value",
        "prop2": "default value"
    },
    "some array": [
        5,
        6
    ],
    "some value": 1.0
})";
    std::string o = R"({
    "nested object1": {
        "prop2": "new value"
    },
    "some value": -12.0
})";
    Dictionary merged = parse_json(defaults).overrideEntries(parse_json(o));
    REQUIRE("default value" == merged.at("nested object1").at("prop1").asString());
    REQUIRE("new value" == merged.at("nested object1").at("prop2").asString());
    REQUIRE(-12.0 == Approx(merged.at("some value").asDouble()));
}

TEST_CASE("Demonstrate how to get a Json object from a dict formatted string") {
    SECTION("Object") {
        std::string settings_string =
                    R"({"pokemon":{"name":"Pikachu", "nicknames":["pika", "pikachu", "yellow rat"]}})";
        auto dict_object = parse_json(settings_string);
        REQUIRE(dict_object.at("pokemon").at("name").asString() == "Pikachu");
    }
    SECTION("Array of objects") {
        std::string settings_string = R"([{"name":"Pikachu"}, {"name":"Mewtwo"}])";
        auto dict_object = parse_json(settings_string);
        REQUIRE(2 == dict_object.size());
        REQUIRE(dict_object.at(0).at("name").asString() == "Pikachu");
        REQUIRE(dict_object.at(1).at("name").asString() == "Mewtwo");
    }
}

TEST_CASE("Pretty print Json") {
    Dictionary dict;
    dict["some array"][0] = 5;
    dict["some array"][1] = 6;
    dict["some value"] = double(1e200);
    dict["empty array"] = std::vector<double>{};
    dict["empty object"] = Dictionary();
    std::string expected = R"({
    "empty array": [],
    "empty object": {},
    "some array": [5,6],
    "some value": 1e+200
})";
    REQUIRE(parse_json(expected) == parse_json(dict.dump(4)));
}

TEST_CASE("Can iterate through objects") {
    std::string config = R"({
    "regions": [
        {"type": "sphere", "radius":1.0, "center":[0,0,0]},
        {"type": "sphere", "radius":1.5, "center":[1,0,0]}
    ]
})";

    auto dict = parse_json(config);
    REQUIRE(dict.keys().size() == 1);

    for (auto r : dict.at("regions").asObjects()) {
        printf("%s\n", r.dump().c_str());
    }
}

TEST_CASE("Can assign a dictionary to a string") {
    std::string config = R"([
        {"type": "sphere", "radius":1.0, "center":[0,0,0]},
        {"type": "sphere", "radius":1.5, "center":[1,0,0]}
    ])";
    auto dict = parse_json(config);
    dict = "dummy";
    REQUIRE(0 == dict.size());
}

TEST_CASE("Dictionary array access") {
    std::string config = R"({
    "regions": [
        {"type": "sphere", "radius":1.0, "center":[0,0,0]},
        {"type": "sphere", "radius":1.5, "center":[1,0,0]}
    ]
})";
    Dictionary d = parse_json(config);
}

TEST_CASE("Empty array gives object as array type") {
    std::string config = R"({
    "empty array": []
})";
    Dictionary d = parse_json(config);
    REQUIRE(d.at("empty array").type() == Dictionary::ObjectArray);
}