# AGENTS.md — Developer Reference for AI Agents

This document provides essential information for AI coding agents working on the **parsec** project. It covers the project's goals, build process, testing infrastructure, and key architectural decisions.

## Project Overview

**parsec** is a human-friendly dictionary parser library for C++ (namespace `ps`) that supports multiple configuration formats including:
- **JSON** (strict, machine-friendly)
- **RON** (Rusty Object Notation — human-friendly with relaxed syntax)
- **TOML** (Tom's Obvious Minimal Language — popular configuration format)
- **INI** (simple, widely-used format for basic configuration)
- **YAML** (work in progress)

### Core Design Goals

1. **Easy to Use**
   - Python-like `Dictionary` and `Value` types for intuitive configuration handling
   - Brace-init support for concise C++ initialization: `Dictionary d = {{"key", "value"}};`
   - Single-include header (`ps/parsec.h`) for quick integration
   - Clean, minimal API surface

2. **Highly Robust Parsing**
   - Handles both strict JSON and relaxed RON syntax
   - Automatic comment stripping (Python-style `#` comments)
   - Permissive features: trailing commas, unquoted identifiers
   - Comprehensive test suite with edge cases

3. **Great Error Messages for Humans**
   - **Line and column precision** in all parse errors
   - **Contextual hints** (e.g., "is there a missing closing quote?")
   - **Visual caret markers** (`^`) pointing to the exact error location
   - **Opener location tracking** for unmatched braces/brackets ("opened at line X, column Y")
   - Designed for CI/CD integration with clear, grep-friendly output

Example error output:
```
JSON parse error: expected ',' or '}' — is there a missing closing quote on 'A string with no end
        "'? (line 3, column 3)
        "other": 1
  ^
(opened at line 1, column 2)
```

## Build System

### Project Structure

- **CMake-based** build system (minimum version 3.16)
- **C++17** standard required
- **Library target**: `parsec_lib` (aliased as `ps::parsec`)
- **CLI tool**: `parsec` executable (optional, enabled by default)
- **Test suite**: `parsec_tests` (uses Catch2 v3.4.0)

### Standard Build (Library Only)

```bash
# Configure and build the library and CLI tool
cd build
cmake .. 
make -j
```

### Build with Testing Enabled

```bash
# Configure and build with testing support in the build directory
cd build
cmake -DPARSEC_BUILD_TESTING=ON ..
make -j

# Run tests from the build directory
cd build
./test/parsec_tests
```

### CMake Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `PARSEC_BUILD_TESTING` | `OFF` | Build the unit test suite (fetches Catch2) |
| `PARSEC_BUILD_CLI` | `ON` | Build the `parsec` command-line tool |
| `PARSEC_ENABLE_STRICT_WARNINGS` | `OFF` | Enable additional compiler warnings (-Wconversion, -Wshadow, etc.) |
| `PARSEC_WARNINGS_AS_ERRORS` | `OFF` | Treat all warnings as errors |

### Build Artifacts

After building:
- **Library**: `build/src/libparsec_lib.a` (static library)
- **CLI tool**: `build/src/parsec` (executable)
- **Tests**: `build/test/parsec_tests` (Catch2 test runner)

## Running Tests

```bash
# Run tests from the build directory
cd build
./test/parsec_tests
```

### Using Catch2 Test Runner Directly

```bash
./build/test/parsec_tests                    # Run all tests
./build/test/parsec_tests --list-tests       # List all test cases
./build/test/parsec_tests --list-tags        # List all tags
./build/test/parsec_tests "[json]"           # Run tests tagged with [json]
./build/test/parsec_tests "Can merge two"    # Run specific test by name substring
```

### Test Organization

Tests are located in `test/` with the following coverage:

- `test_json.cpp` — JSON parsing, merging, pretty-printing
- `test_ron.cpp` — RON format parsing and features
- `test_yaml.cpp` — YAML parsing support
- `test_parse_errors.cpp` — Error message quality and line/column reporting
- `test_validate.cpp` — Schema validation logic
- `test_setdefaults.cpp` — Default value application from schemas
- `test_dictionary.cpp` — Core `Dictionary` API functionality
- `test_initializer.cpp` — Brace-init construction
- `test_pretty_print.cpp` — Compact/formatted output modes
- `test_parse_examples.cpp` — Real-world example files in `examples/`
- `test_comments.cpp` — Comment stripping behavior
- `test_cli_parser_selection.cpp` — CLI format detection logic

## Writing Unit Tests

When adding or modifying C++ code, you **must** also add or update unit tests using Catch2.

### Test File Organization

**Location and Naming**:
- Place tests in the `test/` directory
- Name test files with descriptive suffixes:
  - `test_something.cpp` for unit tests
  - `test_something_integration.cpp` for integration tests
- Mirror source structure when appropriate: `src/foo.cpp` → `test/test_foo.cpp`

**Dependencies**:
- Include `<catch2/catch_test_macros.hpp>` in all test files
- Add additional Catch2 headers (e.g., `<catch2/matchers/catch_matchers.hpp>`) only when needed
- **Do not define `main()`** — the project provides a test runner via Catch2
- Only include headers you actually use
- Prefer including public headers (e.g., `<ps/parsec.h>`) instead of internal implementation files

### Test Structure and Style

**Use `TEST_CASE`, avoid `SECTION`**:
```cpp
#include <catch2/catch_test_macros.hpp>
#include <ps/parsec.h>

TEST_CASE("Dictionary looks up existing keys", "[dictionary][unit]") {
    ps::Dictionary dict;
    dict["answer"] = 42;

    REQUIRE(dict.has("answer"));
    REQUIRE(dict.at("answer").asInt() == 42);
}

TEST_CASE("Dictionary throws on missing keys", "[dictionary][unit][exception]") {
    ps::Dictionary dict;

    REQUIRE_FALSE(dict.has("missing"));
    REQUIRE_THROWS_AS(dict.at("missing"), std::out_of_range);
}
```

**Test case naming**:
- First argument: Clear English description of the behavior being tested
- Second argument: One or more tags in brackets
  - Always include a component tag (e.g., `[json]`, `[dictionary]`, `[validate]`)
  - Always include a category tag: `[unit]`, `[integration]`, `[exception]`, `[slow]`
  - Examples: `[json][unit]`, `[parser][unit][exception]`

### Assertions and Matchers

**Prefer strong, specific assertions**:
- `REQUIRE` — for conditions that must hold for the test to continue
- `CHECK` — for additional checks where failure doesn't invalidate the rest of the test
- `REQUIRE_NOTHROW` — verify code does not throw
- `REQUIRE_THROWS` — verify code throws any exception
- `REQUIRE_THROWS_AS` — verify code throws a specific exception type
- `REQUIRE_THROWS_WITH` — verify exception message content

**Exception testing examples**:
```cpp
TEST_CASE("Parser throws on invalid JSON syntax", "[json][unit][exception]") {
    std::string bad_json = R"({"key": "value)";
    
    REQUIRE_THROWS_AS(ps::parse_json(bad_json), std::runtime_error);
}

TEST_CASE("Parser provides helpful error messages", "[json][unit][exception]") {
    std::string bad_json = R"({"duplicate": 1, "duplicate": 2})";
    
    REQUIRE_THROWS_WITH(
        ps::parse_json(bad_json),
        Catch::Matchers::ContainsSubstring("duplicate")
    );
}
```

**Floating-point comparisons**:
```cpp
#include <catch2/catch_approx.hpp>

TEST_CASE("Parse floating point values", "[json][unit]") {
    auto dict = ps::parse_json(R"({"pi": 3.14159})");
    REQUIRE(dict.at("pi").asDouble() == Catch::Approx(3.14159).epsilon(1e-12));
}
```

### Test Coverage Expectations

When you add or modify code, you **must** also add tests that cover:

1. **Happy path behavior** — the intended use case works correctly
2. **Edge cases** — empty input, null values, boundary conditions
3. **Invalid input** — malformed syntax, wrong types, missing required fields
4. **Error handling** — exceptions are thrown with correct types and messages

**Keep tests small and focused**:
- One behavior per `TEST_CASE`
- Name tests so a failing test clearly indicates what's broken
- Avoid testing multiple unrelated behaviors in a single test case

**Example of focused tests**:
```cpp
TEST_CASE("Can parse empty JSON object", "[json][unit]") {
    auto dict = ps::parse_json("{}");
    REQUIRE(dict.size() == 0);
}

TEST_CASE("Can parse empty JSON array", "[json][unit]") {
    auto dict = ps::parse_json("[]");
    REQUIRE(dict.size() == 0);
}

TEST_CASE("Can parse nested objects", "[json][unit]") {
    auto dict = ps::parse_json(R"({"outer": {"inner": 42}})");
    REQUIRE(dict.at("outer").at("inner").asInt() == 42);
}
```

### Performance and Stability

**Keep unit tests fast and deterministic**:
- Avoid network calls, filesystem I/O, or random behavior unless specifically testing those features
- If randomization is needed, use a fixed seed and document it in a comment
- Tests should run quickly (< 1 second per test case) to enable rapid iteration
- Use `[slow]` tag for tests that take longer than 1 second

**Avoid external dependencies in unit tests**:
- Do not add production-only dependencies (MPI, large external libraries, etc.) unless the test specifically validates integration with that dependency
- Prefer testing against small, in-memory examples rather than large external files

### Common Patterns in parsec Tests

**Parsing tests**:
```cpp
TEST_CASE("Parse JSON with trailing commas", "[json][unit]") {
    std::string json = R"({"a": 1, "b": 2,})";
    REQUIRE_NOTHROW(ps::parse_json(json));
}
```

**Error message quality tests**:
```cpp
TEST_CASE("Error includes line and column", "[json][unit][exception]") {
    std::string bad = "{\n  \"key\": bad_value\n}";
    try {
        ps::parse_json(bad);
        FAIL("Expected parse to throw");
    } catch (const std::exception& e) {
        std::string msg = e.what();
        REQUIRE(msg.find("line") != std::string::npos);
        REQUIRE(msg.find("column") != std::string::npos);
    }
}
```

**Schema validation tests**:
```cpp
TEST_CASE("Validate data against schema", "[validate][unit]") {
    ps::Dictionary schema = ps::parse_json(R"({"type": "object", "required": ["name"]})");
    ps::Dictionary valid_data = ps::parse_json(R"({"name": "test"})");
    ps::Dictionary invalid_data = ps::parse_json(R"({"other": "value"})");
    
    REQUIRE_FALSE(ps::validate(valid_data, schema).has_value());
    REQUIRE(ps::validate(invalid_data, schema).has_value());
}
```

### Adding Tests to the Build

After creating a new test file:

1. Add it to `test/CMakeLists.txt`:
   ```cmake
   add_executable(parsec_tests
       test_json.cpp
       test_ron.cpp
       test_your_new_test.cpp  # Add here
       # ... other test files
   )
   ```

2. Rebuild and run tests:
   ```bash
   cd build
   cmake --build .
   ./test/parsec_tests
   ```

3. Run specific tests to verify:
   ```bash
   ./test/parsec_tests "your test name"
   ./test/parsec_tests "[yourtag]"
   ```


## CLI Tool Usage

The `parsec` executable validates configuration files and provides schema validation:

```bash
# Auto-detect format and validate syntax
./build/src/parsec examples/sample.ron

# Explicitly parse as JSON
./build/src/parsec --json examples/simple.json

# Explicitly parse as RON
./build/src/parsec --ron examples/high.ron

# Explicitly parse as TOML
./build/src/parsec --toml examples/sample.toml

# Explicitly parse as INI
./build/src/parsec --ini examples/sample.ini

# Validate against a JSON schema
./build/src/parsec --validate schemas/simple_schema.json examples/simple.json
```

**Format Hints**: The library supports format hints in the first line of files using common editor conventions:
- Vim modeline: `# vim: set filetype=toml:` or `# vim: ft=json`
- Emacs mode line: `# -*- mode: yaml -*-`
- Simple pragma: `# format: ini`

When a format hint is detected, **only** that parser is used. This provides clear error messages when the file doesn't match its declared format.

**Exit codes:**
- `0` — Success (valid syntax/schema)
- `1` — Parse error or validation failure
- `2` — Usage error (missing arguments)

**Output:**
- On success: Prints `OK: parsed <format>; value: ...` and a preview
- On error: Prints detailed error with line/column and exits non-zero

## Library Integration

### Adding parsec to Your CMake Project

```cmake
add_subdirectory(path/to/parsec)
target_link_libraries(your_target PRIVATE ps::parsec)
```

### Basic Usage Example

```cpp
#include <ps/parsec.h>

int main() {
    // Option 1: Use format-specific parsers
    std::string config = "{ port: 8080, host: localhost }";
    ps::Dictionary d = ps::parse_ron(config);
    
    int port = d.at("port").asInt();
    std::string host = d.at("host").asString();
    
    // Option 2: Use auto-detection (tries all formats)
    ps::Dictionary auto_parsed = ps::parse(config);
    
    // Format hints are also supported in the input:
    // # format: toml
    // # vim: set filetype=json:
    // # -*- mode: yaml -*-
    
    return 0;
}
```

### Schema Validation

```cpp
#include <ps/parsec.h>
#include <ps/validate.h>

ps::Dictionary schema = ps::parse_json(schema_content);
ps::Dictionary data = ps::parse_ron(config_content);

// Validate and get error message if any
auto error = ps::validate(data, schema);
if (error.has_value()) {
    std::cerr << "Validation error: " << *error << '\n';
}

// Apply defaults from schema
ps::Dictionary completed = ps::setDefaults(data, schema);
```

## Key API Surfaces

### Core Types

- `ps::Dictionary` — Unified value container (object, array, scalar)

### Parsing Functions

- `ps::parse_json(const std::string&)` → `Dictionary`
- `ps::parse_ron(const std::string&)` → `Dictionary`
- `ps::parse_toml(const std::string&)` → `Dictionary`
- `ps::parse_ini(const std::string&)` → `Dictionary`
- `ps::parse_yaml(const std::string&)` → `Dictionary`
- `ps::parse_auto(const std::string&, const std::string& hint)` → `Dictionary`

### Validation & Defaults

- `ps::validate(const Dictionary& data, const Dictionary& schema)` → `std::optional<std::string>`
- `ps::setDefaults(const Dictionary& data, const Dictionary& schema)` → `Dictionary`

### Dictionary Methods (Selected)

- `.at(const std::string& key)` / `.at(int index)` — Access by key/index
- `.has(const std::string& key)` — Check key existence
- `.size()` — Number of elements
- `.keys()` — Get all object keys
- `.dump(int indent = 4, bool compact = true)` — Serialize to string
- `.asInt()`, `.asDouble()`, `.asString()`, `.asBool()` — Type conversions
- `.asInts()`, `.asDoubles()`, `.asStrings()`, `.asBools()`, `.asObjects()` — Array conversions
- `.merge(const Dictionary&)` — Deep merge two dictionaries
- `.overrideEntries(const Dictionary&)` — Shallow merge/override

## Important Implementation Notes

### Pretty-Printer Behavior

The `dump()` method has specific behavior relied upon by tests:

```cpp
dict.dump(0, true)    // Compact one-line: {"key":"value","arr":[1,2,3]}
dict.dump(4, true)    // Indented with smart compaction for small structures
dict.dump(4, false)   // Always multi-line indented
```

Tests expect `dump(0, true)` to produce deterministic single-line output with no spaces.

### Array Builder Syntax

Dictionaries support in-place array construction:

```cpp
ps::Dictionary d;
d["arr"][0] = 1;
d["arr"][1] = 2;
// d is now {"arr": [1, 2]}
```

This auto-converts empty objects to arrays on indexed access but throws if the object already has string keys.

### Type Flexibility

Helper methods like `asInts()` accept both arrays and scalars, returning single-element vectors for scalars:

```cpp
Dictionary scalar = 42;
auto vec = scalar.asInts();  // Returns {42}
```

### Test Dependencies

- **Catch2 v3.4.0** is fetched automatically via CMake FetchContent
- Tests use `REQUIRE` and `TEST_CASE` macros
- Example files in `examples/` are used by `test_parse_examples.cpp`
- Tests define `EXAMPLES_DIR` macro pointing to the examples directory

## Error Message Philosophy

When modifying parsing code, maintain these error message standards:

1. **Always include line and column numbers** in exceptions
2. **Provide context snippets** showing the problematic line
3. **Use caret markers (`^`)** to point to the exact error location
4. **Track opener locations** for brackets/braces and report them on mismatch
5. **Suggest fixes** when possible (e.g., "is there a missing closing quote?")
6. **Keep messages grep-friendly** with consistent prefixes like "JSON parse error:" or "RON parse error:"

Example test verifying error quality:

```cpp
TEST_CASE("unmatched opener includes opener location") {
    std::string s = R"({"obj": [1, 2, 3)";
    try {
        parse_json(s);
        FAIL("expected parse to throw");
    } catch (const std::exception& e) {
        REQUIRE(std::string(e.what()).find("opened at") != std::string::npos);
    }
}
```

## Common Development Workflows

### Adding a New Test

1. Add test file to `test/CMakeLists.txt` in `add_executable(parsec_tests ...)`
2. Write test cases using Catch2: `TEST_CASE("description") { ... }`
3. Use `REQUIRE()` assertions for validation
4. Rebuild and run: `cmake --build build && ./build/test/parsec_tests`

### Modifying the Parser

1. Parser implementations are in `src/src/`:
   - `json_parser.cpp` — JSON parsing logic
   - `ron_parser.cpp` — RON parsing logic
   - `toml_parser.cpp` — TOML parsing logic
   - `ini_parser.cpp` — INI parsing logic
   - `yaml_parser.cpp` — YAML parsing logic
2. Ensure error messages include line/column context
3. Add corresponding tests in `test/test_parse_errors.cpp`
4. **Verify the code compiles with zero warnings**: Run `cmake --build build` and check output
5. Run full test suite to verify no regressions

### Adding a New Example File

1. Place file in `examples/` directory
2. Add test case in `test/test_parse_examples.cpp`:
   ```cpp
   TEST_CASE("Parse new example") {
       std::string path = std::string(EXAMPLES_DIR) + "/new_example.ron";
       std::ifstream in(path);
       std::string content((std::istreambuf_iterator<char>(in)), {});
       REQUIRE_NOTHROW(ps::parse_ron(content));
   }
   ```

## Debugging Tips

- **Enable verbose test output**: `./build/test/parsec_tests -s` (shows all output)
- **Run specific test**: `./build/test/parsec_tests "test name substring"`
- **Check error messages**: Look at `test/test_parse_errors.cpp` for expected format
- **Use the CLI for quick validation**: `./build/src/parsec examples/file.ron`
- **Inspect parser behavior**: Add debug prints in `src/src/*_parser.cpp`

## Future Directions (from todo.md)

- More helpful schema validation errors. (Example, key "turblnce" is invalid, did you mean "turbulence model" ?)

---

**Last Updated**: November 2025  
**Maintainer**: matthew-oconnell  
**License**: Apache License 2.0
