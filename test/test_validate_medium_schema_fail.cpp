#include <catch2/catch_all.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "ps/parsec.h"
#include "ps/validate.h"
#include "ps/ron.h"

namespace fs = std::filesystem;
using namespace ps;

static std::string read_file_strip_fence(const fs::path &p) {
    std::ifstream in(p);
    REQUIRE(in.good());
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // strip leading ```... fence if present
    if (content.rfind("```", 0) == 0) {
        auto pos = content.find('\n');
        if (pos != std::string::npos) {
            auto last = content.rfind("```");
            if (last != std::string::npos && last > pos) {
                return content.substr(pos + 1, last - (pos + 1));
            }
        }
    }
    return content;
}

TEST_CASE("validate medium schema fail example", "[validate][medium][fail]") {
    fs::path examples = fs::path(EXAMPLES_DIR);
    fs::path checkp = examples / "medium_schema_fail.ron";
    std::string check_content = read_file_strip_fence(checkp);
    auto v_check = ps::parse_ron(check_content);
    REQUIRE(v_check.isDict());
    Dictionary cfg = *v_check.asDict();

    fs::path schemap = examples.parent_path() / "schemas" / "medium_schema.json";
    std::string schema_content = read_file_strip_fence(schemap);
    auto v_schema = ps::parse(schema_content);
    REQUIRE(v_schema.isDict());
    Dictionary schema = *v_schema.asDict();

    auto err = ps::validate(cfg, schema);
    // This example intentionally violates the schema (version component has leading zero)
    REQUIRE(err.has_value());
}
