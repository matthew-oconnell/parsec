#include <catch2/catch_all.hpp>
#include <ps/json.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("parse example JSON files") {
#ifndef EXAMPLES_DIR
    FAIL("EXAMPLES_DIR not defined");
#endif
    fs::path examples = EXAMPLES_DIR;
    REQUIRE(fs::exists(examples));

    int count = 0;
    for (auto const& e : fs::directory_iterator(examples)) {
        if (!e.is_regular_file()) continue;
        if (e.path().extension() != ".json") continue;
        try {
            std::ifstream in(e.path());
            REQUIRE(in.good());
            std::string content((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
            auto v = ps::parse_json(content);
            REQUIRE(not v.to_string().empty());
            ++count;
        } catch (const std::exception& ex) {
            FAIL("Failed to parse " + e.path().string() + ": " + ex.what());
        }
    }
    REQUIRE(count >= 1);
}
