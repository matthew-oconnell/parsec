
# parsec — human-friendly dictionary parser for C++

Licensed under the Apache License, Version 2.0 — see the `LICENSE` file for details.

parsec is a small, easy-to-use C++ library (namespace `ps`) that provides a "Python-like" dictionary Value/Dictionary type and a parser for human-friendly configuration files. The parser supports multiple formats:
- **JSON** — strict, machine-friendly
- **RON** (Rusty Object Notation) — more relaxed, human-friendly with reduced quoting and trailing-comma flexibility
- **TOML** — popular configuration format with tables, inline tables, and clear syntax
- **INI** — simple, widely-used format for basic configuration
- **YAML** — work in progress

This README shows why parsec's parser is easier for humans, how to validate files with the command-line tool, and a short developer section describing how to integrate the library into your C++ project.

## Why parsec is human-friendly

Parsing strict JSON is great for machines, but it's noisy for humans. RON-style files supported by parsec let you write configuration with less punctuation and better readability.

Example: equivalent JSON vs RON

JSON (strict, verbose):

```json
{
	"simulation": {
		"mach": 7.5,
		"aoa": 10.0,
		"mesh": { "file": "meshes/waverider.cgns" },
		"metadata": { "author": "<author-placeholder>", "contributors": ["Alice", "Bob"] }
	}
}
```

RON (more human-friendly, supported by parsec):

```ron
{
		simulation: {
				mach: 7.5,
				aoa: 10.0,
				mesh: { file: "meshes/waverider.cgns" },
				metadata: { author: "<author-placeholder>", contributors: [Alice, Bob] }
		}
}
```

RON advantages demonstrated above:
- Keys do not always require double quotes when they are simple identifiers.
- Trailing commas are allowed for easier editing.
- Arrays can contain unquoted simple identifiers (useful for short lists and constants).
- Comments and multi-line values (if present) make files more maintainable.

> Note: parsec accepts strict JSON as well, so you can mix or migrate progressively.

## Using the `parsec` CLI to check syntax

The repository provides a small `parsec` command-line binary that validates files and prints a compact parse error with location when something is wrong.

Build and run (out-of-source):

```bash
cmake -S . -B build -DPARSEC_BUILD_TESTS=ON
cmake --build build --parallel
# run the CLI on a file (auto-detect format)
./build/parsec path/to/config.ron
# or explicitly specify format
./build/parsec --json path/to/config.json
./build/parsec --ron path/to/config.ron
./build/parsec --toml path/to/config.toml
./build/parsec --ini path/to/config.ini
```

**Auto-detection**: When you don't specify a format flag, parsec automatically tries all supported formats (JSON, RON, TOML, INI, YAML) and uses intelligent heuristics to report the most helpful error message if all formats fail. For example, if your file starts with `{` and contains `key: value` patterns, parsec will report errors from the JSON parser even if it also tried other formats.

**Format hints**: You can add a format hint in the first line using common editor conventions:
- Vim modeline: `# vim: set filetype=toml:` or `# vim: ft=json`
- Emacs mode line: `# -*- mode: yaml -*-`
- Simple pragma: `# format: ini`

When a format hint is present, parsec uses **only** that parser and reports errors from that format. This ensures clear, unambiguous error messages when the file doesn't match the declared format. If no hint is present, parsec tries all formats automatically.

If the file contains a syntax error the tool prints a helpful message with line/column and exits non-zero. This makes it handy to plug into pre-commit hooks or CI pipelines to ensure configuration correctness.

Schema validation from the CLI

The `parsec` CLI can also validate a configuration against a JSON schema-like dictionary. Use the `--validate` flag and provide the schema (JSON) and the data file (JSON or RON):

```bash
# validate data.ron against schema.json
./build/parsec --validate schema.json data.ron
```

On success the CLI prints `OK: validation passed` and exits 0. On failure it prints a one-line `validation error:` message and exits non-zero so you can integrate it into CI or pre-commit checks.

### Example error messages

Here are two realistic examples of the kind of output `parsec` prints when it encounters a syntax violation. The messages include a one-line explanation, the line with the error, and a caret pointing to the column.

1) Intelligent format detection on error:

File `bad.txt`:

```
true false
```

Command (auto-detect format):

```bash
./build/parsec bad.txt
```

Example stderr output:

```
parse error: Failed to parse as any supported format. Most likely intended format: JSON

Tried to parse this string <true false
> but encountered this error: extra data after JSON value (line 1, column 6)
true false
     ^
```

parsec automatically identified that this looks like JSON (boolean literal at start) and reported the JSON parser's error message with helpful context.

2) Missing closing quote on a string (helpful snippet extraction):

File `bad.json`:

```json
{
	"description": "A string with no end
	"other": 1
}
```

Command:

```bash
./build/parsec --json bad.json
```

Example stderr output:

```
JSON parse error: expected ',' or '}' — is there a missing closing quote on 'A string with no end
				"'? (line 3, column 3)
				"other": 1
	^
(opened at line 1, column 2)
```

These examples match the parser's diagnostics: a descriptive message, the exact line, and a caret pointing to the problematic column. The CLI prints the parser's message prefixed with either "JSON parse error:" or "RON parse error:" so you can easily grep CI logs for parse failures.

## Developer documentation (quick)

The core library target is `parsec` and public headers are under `src/include/ps`.

Minimal usage example (C++):

```cpp
#include <ps/parsec.h>

int main() {
    std::string content = "{ foo: 1, bar: [true, false] }";
    
    // Option 1: Use format-specific parsers
    ps::Dictionary d1 = ps::parse_ron(content);
    ps::Dictionary d2 = ps::parse_json(json_content);
    ps::Dictionary d3 = ps::parse_toml(toml_content);
    ps::Dictionary d4 = ps::parse_ini(ini_content);
    
    // Option 2: Use automatic format detection (tries all formats)
    ps::Dictionary d = ps::parse(content);  // Auto-detects RON/JSON/TOML/INI/YAML
    
    int foo = d.at("foo").asInt();
}
```

**Multi-format parsing**: The `ps::parse()` function tries all supported formats automatically and uses intelligent heuristics to report the most helpful error if all fail. For example, input starting with `{` is more likely to be JSON/RON, and input with `[[tables]]` is more likely TOML. The parser scores each format based on content characteristics and reports errors from the most likely intended format.

Integrating into your CMake project

- Add `parsec` as a subdirectory or install its headers and link against the `parsec` target.

Example CMake snippet when using parsec as a subproject:

```cmake
add_subdirectory(path/to/parsec)
# link your target with parsec
target_link_libraries(myapp PRIVATE parsec)
```

Public headers are located at `src/include/ps` and provide `ps::Value` and `ps::Dictionary` APIs plus the parser entry points `ps::parse_json` and `ps::parse_ron`.

Brace-init (raw brace syntax) in C++

The library supports concise initializer-list construction that relies on implicit conversions to `ps::Value` and `ps::Dictionary`. This is the same style used in the tests and is convenient for small inline configs.

Example (exact form used in `test/test_initializer.cpp`):

```cpp
#include <ps/parsec.h>

using namespace ps;

Dictionary dict = {
	{"key", "value"},
	{"key2", {"my", "array", "is", "cool"}},
	{"key3", {{"obj key", true}}}
};

// Access like a normal Dictionary
REQUIRE(dict.at("key").asString() == "value");
```

This concise form is equivalent to the more verbose explicit `ps::Value` constructions and is recommended for readability in tests and short examples.

Schema validation and defaults (developer)

The library exposes a small schema validation helper and a defaults applier in `ps::validate` and `ps::setDefaults` respectively.

Signatures (in `ps/validate.hpp`):

```cpp
// returns std::nullopt on success, or an error message on failure
std::optional<std::string> ps::validate(const ps::Dictionary& data, const ps::Dictionary& schema);

// returns a new Dictionary where missing properties with `default` in the schema are filled in
ps::Dictionary ps::setDefaults(const ps::Dictionary& data, const ps::Dictionary& schema);
```

Minimal usage examples:

Validate a parsed file against a schema:

```cpp
#include <ps/parsec.h>
#include <ps/validate.h>

// parse content (JSON or RON) and validate against a JSON schema (loaded as JSON)
std::string schema_json = "{ \"type\": \"object\", \"properties\": { \"port\": { \"type\": \"integer\", \"required\": true } } }";
ps::Dictionary schema = ps::parse_json(schema_json);

std::string data_ron = "{ port: 8080 }";
ps::Dictionary data = ps::parse_ron(data_ron);

auto err = ps::validate(data, schema);
if (err.has_value()) std::cerr << "validation error: " << *err << '\n';
else std::cout << "validation passed\n";
```

Apply defaults from a schema to produce a completed configuration:

```cpp
#include <ps/parsec.hpp>
#include <ps/validate.hpp>

ps::Dictionary schema; // build or parse schema as a Dictionary
// example schema with a default
schema["type"] = ps::Value(std::string("object"));
ps::Dictionary props; props["port"] = ps::Value(ps::Dictionary{{"type", ps::Value(std::string("integer"))},{"default", ps::Value(int64_t(8080))}});
schema["properties"] = ps::Value(props);

ps::Dictionary input; // missing port
ps::Dictionary completed = ps::setDefaults(input, schema);
// completed now contains port=8080
```

If you'd like edits to the README style or different examples (more complex RON features, or showing how to produce machine-readable diffs), tell me which examples you prefer and I will update the file.

## Notes on the `ps::Dictionary` API (recent changes)

A few small, developer-facing changes were introduced to `ps::Dictionary` that are useful to be aware of when upgrading code or writing tests:

- Single-include: prefer `#include <ps/parsec.h>` which pulls in the commonly used headers (`dictionary.h`, `parse.h`, `validate.h`).

- `dump(int indent = 4, bool compact = true) const`: the pretty-printer supports a "tight" compact form when `indent == 0 && compact == true` — this produces a single-line JSON-like string with no spaces after commas or colons (many tests depend on this exact formatting). When `compact` is true the printer will also prefer one-line representations for small structures (uses an 80-character heuristic).

- `operator[](int index)`: that's now a convenience for building arrays in-place. If a `ps::Dictionary` is currently an empty mapped object, using `dict["arr"][0] = 5;` will automatically convert the mapped object into an array and resize it to accommodate the indexed access. If the object already contains keys (i.e., is being used as a map) this will throw.

- `asInts()`, `asDoubles()`, `asStrings()`, `asBools()`, `asObjects()`: these helpers return a vector and will accept either a corresponding array type or a scalar, returning a single-element vector for scalars. For example, a `Dictionary` holding the integer `3` will make `asInts()` return `{3}`. `asObjects()` will return a single-element vector containing the object when called on an object value.

- `overrideEntries(const Dictionary&)` and `merge(const Dictionary&)`: small helpers that merge/overlay object entries. `overrideEntries` applies entries from the argument over the receiver, while `merge` returns a deep-merged result. These are convenient for composing configurations and are used by the library's defaults/validation helpers.

These changes are intentionally small and backward-compatible for most code, but they make some common test and construction idioms easier (especially builder-style `dict["arr"][0] = ...` usage and the pretty-printer's deterministic one-line output).
