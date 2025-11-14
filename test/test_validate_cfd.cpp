#include <catch2/catch_all.hpp>
#include "ps/parsec.h"
#include "ps/validate.h"

using namespace ps;

TEST_CASE("validate CFD schema minimal pass", "[validate][cfd]") {
    // Build a minimal schema capturing the top-level required keys from cfd_schema.json
    Dictionary schema;
    Dictionary meshSpec; meshSpec["type"] = Value("string"); meshSpec["required"] = Value(true);
    Dictionary hsSpec; hsSpec["type"] = Value("object"); hsSpec["required"] = Value(true);

    schema["mesh filename"] = Value(meshSpec);
    schema["HyperSolve"] = Value(hsSpec);

    // Build a config that satisfies the minimal schema
    Dictionary cfg;
    cfg["mesh filename"] = Value(std::string("mesh.msh"));
    Dictionary hs;
    // Include some keys that appear in the real schema to be reasonable
    hs["states"] = Value(Dictionary());
    hs["boundary conditions"] = Value(std::vector<Value>{});
    cfg["HyperSolve"] = Value(hs);

    auto err = validate(cfg, schema);
    REQUIRE(!err.has_value());
}
