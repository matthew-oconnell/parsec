#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <array>
#include <sstream>
#include <string>

// Helper to run a command and capture its output
static std::pair<int, std::string> run_command(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    
    // Redirect stderr to stdout so we capture both
    std::string full_cmd = cmd + " 2>&1";
    FILE* pipe = popen(full_cmd.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    
    int exit_code = pclose(pipe);
    // Extract actual exit code from status
    if (WIFEXITED(exit_code)) {
        exit_code = WEXITSTATUS(exit_code);
    }
    
    return {exit_code, result};
}

TEST_CASE("CLI responds to --help flag", "[cli][unit][help]") {
    auto [exit_code, output] = run_command("./src/parsec --help");
    
    REQUIRE(exit_code == 0);
    REQUIRE(output.find("parsec - Parse and validate") != std::string::npos);
    REQUIRE(output.find("USAGE:") != std::string::npos);
    REQUIRE(output.find("OPTIONS:") != std::string::npos);
}

TEST_CASE("CLI responds to -h flag", "[cli][unit][help]") {
    auto [exit_code, output] = run_command("./src/parsec -h");
    
    REQUIRE(exit_code == 0);
    REQUIRE(output.find("parsec - Parse and validate") != std::string::npos);
    REQUIRE(output.find("USAGE:") != std::string::npos);
    REQUIRE(output.find("--help") != std::string::npos);
}

TEST_CASE("CLI help includes format options", "[cli][unit][help]") {
    auto [exit_code, output] = run_command("./src/parsec --help");
    
    REQUIRE(exit_code == 0);
    REQUIRE(output.find("--json") != std::string::npos);
    REQUIRE(output.find("--ron") != std::string::npos);
    REQUIRE(output.find("--toml") != std::string::npos);
    REQUIRE(output.find("--ini") != std::string::npos);
    REQUIRE(output.find("--yaml") != std::string::npos);
}

TEST_CASE("CLI help includes mode descriptions", "[cli][unit][help]") {
    auto [exit_code, output] = run_command("./src/parsec --help");
    
    REQUIRE(exit_code == 0);
    REQUIRE(output.find("--validate") != std::string::npos);
    REQUIRE(output.find("--fill-defaults") != std::string::npos);
    REQUIRE(output.find("--convert") != std::string::npos);
}

TEST_CASE("CLI help includes key sections", "[cli][unit][help]") {
    auto [exit_code, output] = run_command("./src/parsec --help");
    
    REQUIRE(exit_code == 0);
    REQUIRE(output.find("USAGE:") != std::string::npos);
    REQUIRE(output.find("OPTIONS:") != std::string::npos);
}

TEST_CASE("CLI without args suggests help", "[cli][unit][help]") {
    auto [exit_code, output] = run_command("./src/parsec");
    
    REQUIRE(exit_code == 2);
    REQUIRE(output.find("--help") != std::string::npos);
}
