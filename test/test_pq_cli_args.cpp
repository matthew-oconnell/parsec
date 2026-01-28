#include <catch2/catch_test_macros.hpp>
#include <ps/pq/cli_args.h>

// Phase 3.1 - Parse Basic Arguments

TEST_CASE("Parse file and get path", "[pq][cli_args][unit]") {
    const char* argv[] = {"pq", "file.json", "--get", "server/port"};
    int argc = 4;
    
    ps::pq::CliArgs args(argc, argv);
    
    REQUIRE(args.getFilePath() == "file.json");
    REQUIRE(args.getAction() == ps::pq::CliArgs::Action::GET);
    REQUIRE(args.getPath() == "server/port");
}

TEST_CASE("Parse with short form -g", "[pq][cli_args][unit]") {
    const char* argv[] = {"pq", "file.yaml", "-g", "database/host"};
    int argc = 4;
    
    ps::pq::CliArgs args(argc, argv);
    
    REQUIRE(args.getFilePath() == "file.yaml");
    REQUIRE(args.getAction() == ps::pq::CliArgs::Action::GET);
    REQUIRE(args.getPath() == "database/host");
}

TEST_CASE("Parse file only defaults to print", "[pq][cli_args][unit]") {
    const char* argv[] = {"pq", "config.toml"};
    int argc = 2;
    
    ps::pq::CliArgs args(argc, argv);
    
    REQUIRE(args.getFilePath() == "config.toml");
    REQUIRE(args.getAction() == ps::pq::CliArgs::Action::PRINT);
}

TEST_CASE("No arguments shows help", "[pq][cli_args][unit]") {
    const char* argv[] = {"pq"};
    int argc = 1;
    
    ps::pq::CliArgs args(argc, argv);
    
    REQUIRE(args.getAction() == ps::pq::CliArgs::Action::HELP);
}

TEST_CASE("Path with spaces", "[pq][cli_args][unit]") {
    const char* argv[] = {"pq", "file.json", "--get", "server config/port number"};
    int argc = 4;
    
    ps::pq::CliArgs args(argc, argv);
    
    REQUIRE(args.getPath() == "server config/port number");
}

// Phase 3.2 - Parse Default Values

TEST_CASE("Parse default value", "[pq][cli_args][unit]") {
    const char* argv[] = {"pq", "file.json", "--get", "timeout", "--default", "30"};
    int argc = 6;
    
    ps::pq::CliArgs args(argc, argv);
    
    REQUIRE(args.getAction() == ps::pq::CliArgs::Action::GET);
    REQUIRE(args.getPath() == "timeout");
    REQUIRE(args.hasDefault());
    REQUIRE(args.getDefault() == "30");
}

TEST_CASE("Parse default with short form -d", "[pq][cli_args][unit]") {
    const char* argv[] = {"pq", "file.json", "-g", "port", "-d", "8080"};
    int argc = 6;
    
    ps::pq::CliArgs args(argc, argv);
    
    REQUIRE(args.hasDefault());
    REQUIRE(args.getDefault() == "8080");
}

TEST_CASE("Default can be string with spaces", "[pq][cli_args][unit]") {
    const char* argv[] = {"pq", "file.json", "--get", "key", "--default", "hello world"};
    int argc = 6;
    
    ps::pq::CliArgs args(argc, argv);
    
    REQUIRE(args.getDefault() == "hello world");
}

TEST_CASE("No default returns false for hasDefault", "[pq][cli_args][unit]") {
    const char* argv[] = {"pq", "file.json", "--get", "path"};
    int argc = 4;
    
    ps::pq::CliArgs args(argc, argv);
    
    REQUIRE_FALSE(args.hasDefault());
}

// Phase 3.3 - Parse Additional Flags

TEST_CASE("Parse count action", "[pq][cli_args][unit]") {
    const char* argv[] = {"pq", "file.json", "--count", "users"};
    int argc = 4;
    
    ps::pq::CliArgs args(argc, argv);
    
    REQUIRE(args.getAction() == ps::pq::CliArgs::Action::COUNT);
    REQUIRE(args.getPath() == "users");
}

TEST_CASE("Parse has action", "[pq][cli_args][unit]") {
    const char* argv[] = {"pq", "file.json", "--has", "debug/enabled"};
    int argc = 4;
    
    ps::pq::CliArgs args(argc, argv);
    
    REQUIRE(args.getAction() == ps::pq::CliArgs::Action::HAS);
    REQUIRE(args.getPath() == "debug/enabled");
}

TEST_CASE("Parse as-json flag", "[pq][cli_args][unit]") {
    const char* argv[] = {"pq", "file.json", "--get", "server", "--as-json"};
    int argc = 5;
    
    ps::pq::CliArgs args(argc, argv);
    
    REQUIRE(args.getAction() == ps::pq::CliArgs::Action::GET);
    REQUIRE(args.outputAsJson());
}

TEST_CASE("Output as json defaults to false", "[pq][cli_args][unit]") {
    const char* argv[] = {"pq", "file.json", "--get", "path"};
    int argc = 4;
    
    ps::pq::CliArgs args(argc, argv);
    
    REQUIRE_FALSE(args.outputAsJson());
}

TEST_CASE("Missing path after --get throws", "[pq][cli_args][unit][exception]") {
    const char* argv[] = {"pq", "file.json", "--get"};
    int argc = 3;
    
    REQUIRE_THROWS_AS(ps::pq::CliArgs(argc, argv), std::invalid_argument);
}

TEST_CASE("Missing default value throws", "[pq][cli_args][unit][exception]") {
    const char* argv[] = {"pq", "file.json", "--get", "path", "--default"};
    int argc = 5;
    
    REQUIRE_THROWS_AS(ps::pq::CliArgs(argc, argv), std::invalid_argument);
}
