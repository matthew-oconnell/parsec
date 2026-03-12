#include <catch2/catch_test_macros.hpp>
#include <ps/pq/text_editor.h>
#include <ps/pq/path_parser.h>
#include <ps/parsec.h>
#include <fstream>
#include <string>

static std::vector<ps::pq::PathToken> parse_path(const std::string& path) {
    ps::pq::PathParser parser;
    return parser.parse(path);
}

// ── prepend ────────────────────────────────────────────────────────────

TEST_CASE("Prepend to array preserves comments", "[pq][text_editor][unit]") {
    std::string text = R"({
    // server list
    "servers": [
        {"host": "alpha", "port": 8080},
        {"host": "beta", "port": 9090}
    ]
})";

    auto result = ps::pq::TextEditor::prependToArray(
        text, parse_path("servers"), R"({"host":"gamma","port":7070})");

    // Comment is preserved
    REQUIRE(result.find("// server list") != std::string::npos);
    // New element comes before alpha
    auto gamma_pos = result.find("gamma");
    auto alpha_pos = result.find("alpha");
    REQUIRE(gamma_pos < alpha_pos);
    // Result is still valid JSON (parsec can parse it)
    REQUIRE_NOTHROW(ps::parse(result));
    auto d = ps::parse(result);
    REQUIRE(d.at("servers").size() == 3);
    REQUIRE(d.at("servers").at(0).at("host").asString() == "gamma");
}

TEST_CASE("Append to array preserves comments", "[pq][text_editor][unit]") {
    std::string text = R"({
    // server list
    "servers": [
        {"host": "alpha", "port": 8080},
        {"host": "beta", "port": 9090}
    ]
})";

    auto result = ps::pq::TextEditor::appendToArray(
        text, parse_path("servers"), R"({"host":"gamma","port":7070})");

    REQUIRE(result.find("// server list") != std::string::npos);
    // New element comes after beta
    auto beta_pos = result.find("beta");
    auto gamma_pos = result.find("gamma");
    REQUIRE(gamma_pos > beta_pos);
    REQUIRE_NOTHROW(ps::parse(result));
    auto d = ps::parse(result);
    REQUIRE(d.at("servers").size() == 3);
    REQUIRE(d.at("servers").at(2).at("host").asString() == "gamma");
}

TEST_CASE("Prepend to empty array", "[pq][text_editor][unit]") {
    std::string text = R"({
    "items": []
})";

    auto result = ps::pq::TextEditor::prependToArray(
        text, parse_path("items"), "42");

    REQUIRE_NOTHROW(ps::parse(result));
    auto d = ps::parse(result);
    REQUIRE(d.at("items").size() == 1);
    REQUIRE(d.at("items").at(0).asInt() == 42);
}

TEST_CASE("Append to empty array", "[pq][text_editor][unit]") {
    std::string text = R"({
    "items": []
})";

    auto result = ps::pq::TextEditor::appendToArray(
        text, parse_path("items"), R"("hello")");

    REQUIRE_NOTHROW(ps::parse(result));
    auto d = ps::parse(result);
    REQUIRE(d.at("items").size() == 1);
    REQUIRE(d.at("items").at(0).asString() == "hello");
}

TEST_CASE("Append preserves trailing comma style", "[pq][text_editor][unit]") {
    std::string text = R"({
    "items": [
        1,
        2,
    ]
})";

    auto result = ps::pq::TextEditor::appendToArray(
        text, parse_path("items"), "3");

    // Verify the new element appears after 2 and has a trailing comma
    auto pos_2 = result.find("2,");
    auto pos_3 = result.find("3,");
    REQUIRE(pos_2 != std::string::npos);
    REQUIRE(pos_3 != std::string::npos);
    REQUIRE(pos_3 > pos_2);
    // Verify closing bracket is still present
    REQUIRE(result.find("]") != std::string::npos);
}

TEST_CASE("Prepend to nested array", "[pq][text_editor][unit]") {
    std::string text = R"({
    "config": {
        # important settings
        "ports": [80, 443]
    }
})";

    auto result = ps::pq::TextEditor::prependToArray(
        text, parse_path("config/ports"), "22");

    REQUIRE(result.find("# important settings") != std::string::npos);
    REQUIRE_NOTHROW(ps::parse(result));
    auto d = ps::parse(result);
    REQUIRE(d.at("config").at("ports").size() == 3);
    REQUIRE(d.at("config").at("ports").at(0).asInt() == 22);
}

TEST_CASE("Append to nested array", "[pq][text_editor][unit]") {
    std::string text = R"({
    "config": {
        # important settings
        "ports": [80, 443]
    }
})";

    auto result = ps::pq::TextEditor::appendToArray(
        text, parse_path("config/ports"), "8080");

    REQUIRE(result.find("# important settings") != std::string::npos);
    REQUIRE_NOTHROW(ps::parse(result));
    auto d = ps::parse(result);
    REQUIRE(d.at("config").at("ports").size() == 3);
    REQUIRE(d.at("config").at("ports").at(2).asInt() == 8080);
}

TEST_CASE("Preserves block comments", "[pq][text_editor][unit]") {
    std::string text = R"({
    /* This is a block comment
       spanning multiple lines */
    "data": [1, 2, 3]
})";

    auto result = ps::pq::TextEditor::prependToArray(
        text, parse_path("data"), "0");

    REQUIRE(result.find("/* This is a block comment") != std::string::npos);
    REQUIRE(result.find("spanning multiple lines */") != std::string::npos);
    REQUIRE_NOTHROW(ps::parse(result));
    auto d = ps::parse(result);
    REQUIRE(d.at("data").at(0).asInt() == 0);
}

TEST_CASE("Preserves commented-out lines", "[pq][text_editor][unit]") {
    std::string text = R"({
    "modes": [
        {"type": "fast"},
//      {"type": "slow"},
        {"type": "medium"}
    ]
})";

    auto result = ps::pq::TextEditor::appendToArray(
        text, parse_path("modes"), R"({"type":"turbo"})");

    REQUIRE(result.find("//      {\"type\": \"slow\"}") != std::string::npos);
    REQUIRE_NOTHROW(ps::parse(result));
    auto d = ps::parse(result);
    REQUIRE(d.at("modes").size() == 3);
    REQUIRE(d.at("modes").at(2).at("type").asString() == "turbo");
}

TEST_CASE("Prepend to with-comments.json preserves all comments", "[pq][text_editor][integration]") {
    std::string path = std::string(EXAMPLES_DIR) + "/with-comments.json";
    std::ifstream in(path);
    REQUIRE(in.is_open());
    std::string content((std::istreambuf_iterator<char>(in)), {});

    auto result = ps::pq::TextEditor::prependToArray(
        content,
        parse_path("boundary conditions"),
        R"({"mesh boundary tags":"*new","type":"new type"})");

    // All original comments preserved
    REQUIRE(result.find("// 999 Seconds") != std::string::npos);
    REQUIRE(result.find("//      \"mass fractions\"") != std::string::npos);
    REQUIRE(result.find("//    \"chemistry model\": \"finite-rate\"") != std::string::npos);
    REQUIRE(result.find("//    \"chemistry model\": \"frozen\"") != std::string::npos);
    REQUIRE(result.find("//    \"type\": \"laminar\"") != std::string::npos);
    REQUIRE(result.find("//\"restart from iteration\": 1") != std::string::npos);

    // Result is still parseable
    REQUIRE_NOTHROW(ps::parse(result));
    auto d = ps::parse(result);
    REQUIRE(d.at("boundary conditions").size() == 6);
    REQUIRE(d.at("boundary conditions").at(0).at("type").asString() == "new type");
}

TEST_CASE("Append to with-comments.json preserves all comments", "[pq][text_editor][integration]") {
    std::string path = std::string(EXAMPLES_DIR) + "/with-comments.json";
    std::ifstream in(path);
    REQUIRE(in.is_open());
    std::string content((std::istreambuf_iterator<char>(in)), {});

    auto result = ps::pq::TextEditor::appendToArray(
        content,
        parse_path("boundary conditions"),
        R"({"mesh boundary tags":"*exit","type":"outflow"})");

    // All original comments preserved
    REQUIRE(result.find("// 999 Seconds") != std::string::npos);
    REQUIRE(result.find("//    \"type\": \"laminar\"") != std::string::npos);

    REQUIRE_NOTHROW(ps::parse(result));
    auto d = ps::parse(result);
    REQUIRE(d.at("boundary conditions").size() == 6);
    REQUIRE(d.at("boundary conditions").at(5).at("type").asString() == "outflow");
}

TEST_CASE("Path not pointing to array throws", "[pq][text_editor][unit][exception]") {
    std::string text = R"({"name": "test"})";

    REQUIRE_THROWS(ps::pq::TextEditor::prependToArray(
        text, parse_path("name"), "42"));
}

TEST_CASE("Missing key throws", "[pq][text_editor][unit][exception]") {
    std::string text = R"({"items": [1, 2]})";

    REQUIRE_THROWS(ps::pq::TextEditor::prependToArray(
        text, parse_path("missing"), "42"));
}
