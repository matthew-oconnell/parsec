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
    // Parse RON format
    std::string config = "{ port: 8080, host: localhost }";
    ps::Dictionary d = ps::parse_ron(config);
    
    int port = d.at("port").asInt();
    std::string host = d.at("host").asString();
    
    // Or parse JSON
    ps::Dictionary json = ps::parse_json("{\"enabled\": true}");
    bool enabled = json.at("enabled").asBool();
    
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
4. Run full test suite to verify no regressions

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
