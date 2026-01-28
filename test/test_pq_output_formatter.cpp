#include <catch2/catch_test_macros.hpp>
#include <ps/pq/output_formatter.h>
#include <ps/parsec.h>

// Phase 4.1 - Format Raw Values

TEST_CASE("Format integer as raw", "[pq][output_formatter][unit]") {
    ps::Dictionary value = 42;
    ps::pq::OutputFormatter formatter;
    
    REQUIRE(formatter.formatRaw(value) == "42");
}

TEST_CASE("Format double as raw", "[pq][output_formatter][unit]") {
    ps::Dictionary value = 3.14;
    ps::pq::OutputFormatter formatter;
    
    REQUIRE(formatter.formatRaw(value) == "3.14");
}

TEST_CASE("Format string as raw", "[pq][output_formatter][unit]") {
    ps::Dictionary value = "hello";
    ps::pq::OutputFormatter formatter;
    
    REQUIRE(formatter.formatRaw(value) == "hello");
}

TEST_CASE("Format boolean true as raw", "[pq][output_formatter][unit]") {
    ps::Dictionary value = true;
    ps::pq::OutputFormatter formatter;
    
    REQUIRE(formatter.formatRaw(value) == "true");
}

TEST_CASE("Format boolean false as raw", "[pq][output_formatter][unit]") {
    ps::Dictionary value = false;
    ps::pq::OutputFormatter formatter;
    
    REQUIRE(formatter.formatRaw(value) == "false");
}

// Phase 4.2 - Format Arrays (Multiple Values)

TEST_CASE("Format array of strings", "[pq][output_formatter][unit]") {
    std::vector<ps::Dictionary> values;
    values.push_back(ps::Dictionary("Alice"));
    values.push_back(ps::Dictionary("Bob"));
    values.push_back(ps::Dictionary("Charlie"));
    
    ps::pq::OutputFormatter formatter;
    
    REQUIRE(formatter.formatRaw(values) == "Alice\nBob\nCharlie");
}

TEST_CASE("Format array of integers", "[pq][output_formatter][unit]") {
    std::vector<ps::Dictionary> values;
    values.push_back(ps::Dictionary(1));
    values.push_back(ps::Dictionary(2));
    values.push_back(ps::Dictionary(3));
    
    ps::pq::OutputFormatter formatter;
    
    REQUIRE(formatter.formatRaw(values) == "1\n2\n3");
}

TEST_CASE("Format empty array", "[pq][output_formatter][unit]") {
    std::vector<ps::Dictionary> values;
    
    ps::pq::OutputFormatter formatter;
    
    REQUIRE(formatter.formatRaw(values) == "");
}

TEST_CASE("Format single-element array", "[pq][output_formatter][unit]") {
    std::vector<ps::Dictionary> values;
    values.push_back(ps::Dictionary("only"));
    
    ps::pq::OutputFormatter formatter;
    
    REQUIRE(formatter.formatRaw(values) == "only");
}

// Phase 4.3 - Format as JSON

TEST_CASE("Format object as JSON", "[pq][output_formatter][unit]") {
    ps::Dictionary value;
    value["a"] = 1;
    value["b"] = 2;
    
    ps::pq::OutputFormatter formatter;
    std::string result = formatter.formatJson(value);
    
    // Should be valid JSON with both keys
    REQUIRE(result.find("\"a\"") != std::string::npos);
    REQUIRE(result.find("\"b\"") != std::string::npos);
}

TEST_CASE("Format scalar as JSON", "[pq][output_formatter][unit]") {
    ps::Dictionary value = 42;
    
    ps::pq::OutputFormatter formatter;
    
    REQUIRE(formatter.formatJson(value) == "42");
}

TEST_CASE("Format string as JSON", "[pq][output_formatter][unit]") {
    ps::Dictionary value = "hello";
    
    ps::pq::OutputFormatter formatter;
    
    REQUIRE(formatter.formatJson(value) == "\"hello\"");
}

TEST_CASE("Format array as JSON", "[pq][output_formatter][unit]") {
    std::vector<ps::Dictionary> values;
    values.push_back(ps::Dictionary("Alice"));
    values.push_back(ps::Dictionary("Bob"));
    
    ps::pq::OutputFormatter formatter;
    std::string result = formatter.formatJson(values);
    
    REQUIRE(result.find("Alice") != std::string::npos);
    REQUIRE(result.find("Bob") != std::string::npos);
}
