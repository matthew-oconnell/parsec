#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <chrono>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

namespace fs = std::filesystem;

static int run_cmd(const std::string& cmd) {
    // std::system returns implementation-defined status encoding; in tests we only
    // need to distinguish success (0) vs failure (non-zero).
    return std::system(cmd.c_str());
}

TEST_CASE("CLI --convert defaults output path by extension", "[cli][convert][integration]") {
#ifndef PARSEC_EXE_PATH
    FAIL("PARSEC_EXE_PATH not defined");
#else
    const std::string exe = PARSEC_EXE_PATH;

    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
#if defined(__unix__) || defined(__APPLE__)
    const auto pid = static_cast<long long>(::getpid());
#else
    const auto pid = 0LL;
#endif
    const fs::path tmp =
                fs::temp_directory_path() / ("parsec-convert-" + std::to_string(pid) + "-" +
                                             std::to_string(static_cast<long long>(now)));
    fs::create_directories(tmp);

    const fs::path in_json = tmp / "input.json";
    const fs::path out_yaml = tmp / "input.yaml";
    const fs::path schema = tmp / "schema.json";

    {
        std::ofstream out(in_json);
        REQUIRE(out.good());
        out << R"({"name":"vulcan","port":8080,"debug":true,"vec":[1,2,3],"tags":["a","b"]})";
    }

    {
        std::ofstream out(schema);
        REQUIRE(out.good());
        out << R"({
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "properties": {
    "name": { "type": "string" },
    "port": { "type": "integer" },
        "debug": { "type": "boolean" },
        "vec": { "type": "array", "items": { "type": "integer" } },
        "tags": { "type": "array", "items": { "type": "string" } }
  },
  "required": ["name", "port"]
})";
    }

    REQUIRE_FALSE(fs::exists(out_yaml));

    {
        std::ostringstream cmd;
        cmd << '"' << exe << '"' << " --convert yaml " << '"' << in_json.string() << '"';
        const int rc = run_cmd(cmd.str());
        REQUIRE(rc == 0);
    }

    REQUIRE(fs::exists(out_yaml));

    {
        std::ostringstream cmd;
        cmd << '"' << exe << '"' << " --validate " << '"' << schema.string() << '"' << ' ' << '"'
            << out_yaml.string() << '"';
        const int rc = run_cmd(cmd.str());
        REQUIRE(rc == 0);
    }

    fs::remove_all(tmp);
#endif
}
