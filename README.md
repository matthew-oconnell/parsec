
# parsec — human-friendly dictionary parser for C++

parsec is a small, easy-to-use C++ library (namespace `ps`) that provides a "Python-like" dictionary Value/Dictionary type and a parser for human-friendly configuration files. The parser accepts both JSON and a more relaxed, readable format (RON — Rusty Object Notation) that reduces quoting, trailing-comma pain, and visual clutter.

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
# run the CLI on a file
./build/parsec path/to/config.ron
# or validate a JSON file
./build/parsec path/to/config.json
```

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

1) Common mistake: using Python-style `True`/`False` instead of JSON `true`/`false`.

File `config.ron`:

```ron
{
	enabled: True
}
```

Command (explicitly parse as RON):

```bash
./build/parsec --ron config.ron
```

Example stderr output:

```
OK: parsed RON; value: { "enabled": "True" }
Preview: {object, 1 keys} enabled="True"
```

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
#include <ps/dictionary.hpp>

int main() {
		std::string content = "{ foo: 1, bar: [true, false] }";
		auto v = ps::parse_ron(content); // or ps::parse_json(content)
		if (v.isDict()) {
				auto d = *v.asDict();
				int foo = d.at("foo").asInt();
		}
}
```

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
#include <ps/parsec.hpp>

using namespace ps;

Dictionary dict = {
	{"key", "value"},
	{"key2", {"my", "array", "is", "cool"}},
	{"key3", {{"obj key", true}}}
};

// Access like a normal Dictionary/Value
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
#include <ps/parsec.hpp>
#include <ps/validate.hpp>

// parse content (JSON or RON) and validate against a JSON schema (loaded as JSON)
std::string schema_json = "{ \"type\": \"object\", \"properties\": { \"port\": { \"type\": \"integer\", \"required\": true } } }";
auto s = ps::parse_json(schema_json);
ps::Dictionary schema = *s.asDict();

std::string data_ron = "{ port: 8080 }";
ps::Value v = ps::parse_ron(data_ron);
ps::Dictionary data = *v.asDict();

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
