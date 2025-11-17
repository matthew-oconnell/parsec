#include <catch2/catch_all.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "ps/parsec.h"
#include "ps/validate.h"
#include "ps/ron.h"

namespace fs = std::filesystem;
using namespace ps;

static std::string read_file_strip_fence(const fs::path& p) {
    std::ifstream in(p);
    REQUIRE(in.good());
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // strip leading ```... fence if present
    if (content.rfind("```", 0) == 0) {
        // find first newline
        auto pos = content.find('\n');
        if (pos != std::string::npos) {
            // find last fence
            auto last = content.rfind("```");
            if (last != std::string::npos && last > pos) {
                return content.substr(pos + 1, last - (pos + 1));
            }
        }
    }
    return content;
}

TEST_CASE("validate medium schema against example check file", "[validate][medium]") {
    fs::path examples = fs::path(EXAMPLES_DIR);
    // example check is stored in examples/medium_schema_check.ron
    fs::path checkp = examples / "medium_schema_check.ron";
    std::string check_content = read_file_strip_fence(checkp);
    Dictionary cfg = ps::parse_ron(check_content);

    // schema is in repository schemas/medium_schema.json (adjacent to examples dir)
    fs::path schemap = examples.parent_path() / "schemas" / "medium_schema.json";
    std::string schema_content = read_file_strip_fence(schemap);
    auto schema = ps::parse(schema_content);

    auto err = ps::validate(cfg, schema);
    REQUIRE(!err.has_value());
}
