#include <catch2/catch_test_macros.hpp>
#include "ps/parsec.h"
#include "ps/validate.h"

using namespace ps;

TEST_CASE("deprecated property is reported during validation", "[validate][deprecated]") {
    // Schema with a deprecated property
    Dictionary schema;
    schema["type"] = "object";

    Dictionary props;

    // Current property
    Dictionary portProp;
    portProp["type"] = "integer";
    portProp["description"] = "The server port number";
    props["port"] = portProp;

    // Deprecated property
    Dictionary serverPortProp;
    serverPortProp["type"] = "integer";
    serverPortProp["deprecated"] = true;
    serverPortProp["description"] =
                "DEPRECATED: Use 'port' instead. This property will be removed in v2.0.";
    props["server_port"] = serverPortProp;

    schema["properties"] = props;
    schema["additionalProperties"] = false;

    SECTION("using current property passes validation") {
        Dictionary data;
        data["port"] = 8080;

        auto err = validate(data, schema);
        REQUIRE_FALSE(err.has_value());
    }

    SECTION("using deprecated property returns error with description") {
        Dictionary data;
        data["server_port"] = 8080;

        auto err = validate(data, schema);
        REQUIRE(err.has_value());

        std::string msg = err.value();
        REQUIRE(msg.find("deprecated") != std::string::npos);
        REQUIRE(msg.find("server_port") != std::string::npos);
        REQUIRE(msg.find("Use 'port' instead") != std::string::npos);
    }
}

TEST_CASE("deprecated enum value in oneOf is reported", "[validate][deprecated][enum]") {
    Dictionary schema;
    schema["type"] = "object";

    Dictionary props;
    Dictionary bcProp;

    std::vector<Dictionary> alternatives;

    // Current BC type
    Dictionary currentBC;
    currentBC["const"] = "supersonic inflow";
    currentBC["description"] = "Supersonic inflow boundary condition";
    alternatives.push_back(currentBC);

    // Deprecated BC type
    Dictionary deprecatedBC;
    deprecatedBC["const"] = "dirichlet";
    deprecatedBC["deprecated"] = true;
    deprecatedBC["description"] =
                "DEPRECATED: Use 'supersonic inflow' instead. Renamed for clarity.";
    alternatives.push_back(deprecatedBC);

    bcProp["oneOf"] = alternatives;
    props["boundary_type"] = bcProp;

    schema["properties"] = props;

    SECTION("using current enum value passes") {
        Dictionary data;
        data["boundary_type"] = "supersonic inflow";

        auto err = validate(data, schema);
        REQUIRE_FALSE(err.has_value());
    }

    SECTION("using deprecated enum value returns error with description") {
        Dictionary data;
        data["boundary_type"] = "dirichlet";

        auto err = validate(data, schema);
        REQUIRE(err.has_value());

        std::string msg = err.value();
        REQUIRE(msg.find("deprecated") != std::string::npos);
        REQUIRE(msg.find("dirichlet") != std::string::npos);
        REQUIRE(msg.find("Use 'supersonic inflow' instead") != std::string::npos);
    }
}
