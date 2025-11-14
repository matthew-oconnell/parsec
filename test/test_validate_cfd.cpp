#include <catch2/catch_all.hpp>
#include "ps/parsec.h"
#include "ps/validate.h"

using namespace ps;

TEST_CASE("validate CFD schema minimal pass", "[validate][cfd]") {
    // Build a minimal schema capturing the top-level required keys from cfd_schema.json
    Dictionary schema;
    Dictionary meshSpec; meshSpec["type"] = "string"; 
    meshSpec["required"] = true;
    Dictionary hsSpec; hsSpec["type"] = "object"; hsSpec["required"] = true;

    schema["mesh filename"] = meshSpec;
    schema["HyperSolve"] = hsSpec;
    // Build a config that satisfies the minimal schema
    Dictionary cfg;
    cfg["mesh filename"] = std::string("mesh.msh");
    Dictionary hs;
    // Include some keys that appear in the real schema to be reasonable
    hs["states"] = Dictionary();
    hs["boundary conditions"] = std::vector<Dictionary>{};
    cfg["HyperSolve"] = hs;

    auto err = validate(cfg, schema);
    REQUIRE(!err.has_value());
}
