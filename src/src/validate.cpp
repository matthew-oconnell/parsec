#include <limits>
#include <string>
#include "ps/validate.h"
#include <ps/ron.h>
#include <sstream>
#include <vector>
#include <algorithm>
#include <regex>
#include <set>
#include <cstdlib>
#include <iostream>

namespace ps {
// Helper: convert string to lowercase
static std::string to_lower(const std::string& s) {
    std::string result = s;
    for (char& c : result) {
        c = std::tolower(static_cast<unsigned char>(c));
    }
    return result;
}

// Helper: compute Levenshtein distance between two strings
static int levenshtein_distance(const std::string& a, const std::string& b) {
    size_t n = a.size();
    size_t m = b.size();
    if (n == 0) return (int)m;
    if (m == 0) return (int)n;
    std::vector<int> prev(m + 1), cur(m + 1);
    for (size_t j = 0; j <= m; ++j) prev[j] = (int)j;
    for (size_t i = 1; i <= n; ++i) {
        cur[0] = (int)i;
        for (size_t j = 1; j <= m; ++j) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost});
        }
        prev.swap(cur);
    }
    return prev[m];
}

// Helper: find nearby keys from a properties dictionary. Returns at most
// `max_suggestions` keys sorted by increasing distance. Uses a normalized
// distance threshold (ratio <= threshold) or small absolute distance.
// Also checks for prefix/substring matches.
static std::vector<std::string> find_nearby_keys(const std::string& key,
                                                 const Dictionary* properties,
                                                 double threshold = 0.40,
                                                 int max_suggestions = 5) {
    std::vector<std::pair<int, std::string>> cands;
    if (!properties) return {};
    for (auto const& p : properties->items()) {
        const std::string& cand = p.first;

        // Check if key is a prefix of candidate (e.g., "norm" is prefix of "norm order")
        bool is_prefix = (cand.size() > key.size() && cand.substr(0, key.size()) == key);
        // Check if key is contained in candidate (substring match)
        bool is_substring = cand.find(key) != std::string::npos;

        int d = levenshtein_distance(key, cand);
        size_t maxlen = std::max(key.size(), cand.size());
        double ratio = maxlen == 0 ? 0.0 : static_cast<double>(d) / static_cast<double>(maxlen);

        // Accept if: ratio small enough, absolute distance small, or prefix/substring match
        if (ratio <= threshold || d <= 2 || is_prefix) {
            // Prioritize prefix matches by giving them distance 0
            if (is_prefix) {
                cands.emplace_back(0, cand);
            } else if (is_substring && d > 3) {
                // Give substring matches a bonus (better distance)
                cands.emplace_back(d / 2, cand);
            } else {
                cands.emplace_back(d, cand);
            }
        }
    }
    if (cands.empty()) return {};
    std::sort(cands.begin(), cands.end(), [](auto const& a, auto const& b) {
        if (a.first != b.first) return a.first < b.first;
        return a.second < b.second;
    });
    std::vector<std::string> out;
    for (size_t i = 0; i < cands.size() && (int)out.size() < max_suggestions; ++i)
        out.push_back(cands[i].second);
    return out;
}

// Helper: limit error message to 80 characters (truncate with ...)
static std::string limit_line_length(const std::string& msg, size_t maxlen = 80) {
    if (msg.size() <= maxlen) return msg;
    return msg.substr(0, maxlen - 3) + "...";
}

// Helper: extract a human-readable name from a schema (for error messages)
static std::string extract_schema_name(const Dictionary& schema, const Dictionary& schema_root) {
    (void)schema_root;  // suppress unused-parameter warning; kept for future use
    // Check for $ref and extract the name
    if (schema.has("$ref") && schema.at("$ref").type() == Dictionary::String) {
        std::string ref = schema.at("$ref").asString();
        size_t pos = ref.rfind('/');
        if (pos != std::string::npos) {
            return ref.substr(pos + 1);
        }
        return ref;
    }

    // Check for a title field
    if (schema.has("title") && schema.at("title").type() == Dictionary::String) {
        return schema.at("title").asString();
    }

    // Check for type-based name
    if (schema.has("type") && schema.at("type").type() == Dictionary::String) {
        return schema.at("type").asString() + " type";
    }

    return "schema";
}

// Forward declaration for recursive use
static const Dictionary* resolve_local_ref(const Dictionary& root, const std::string& ref);

// Helper: generate a minimal example from a schema (for error messages)
[[maybe_unused]] static std::string generate_schema_example(const Dictionary& schema,
                                                            const Dictionary& schema_root,
                                                            int depth = 0) {
    std::string ex;

    // Prevent infinite recursion
    if (depth > 3) return "{...}";

    // Handle $ref by resolving and showing the reference name + structure
    if (schema.has("$ref") && schema.at("$ref").type() == Dictionary::String) {
        std::string ref = schema.at("$ref").asString();
        // Extract just the name from the reference
        size_t pos = ref.rfind('/');
        std::string refName = (pos != std::string::npos) ? ref.substr(pos + 1) : ref;

        // Try to resolve and generate example from the target
        const Dictionary* target = resolve_local_ref(schema_root, ref);
        if (target && depth < 2) {
            return refName + ": " + generate_schema_example(*target, schema_root, depth + 1);
        }
        return refName;
    }

    // Check for type
    if (schema.has("type") && schema.at("type").type() == Dictionary::String) {
        std::string t = schema.at("type").asString();
        if (t == "object") {
            ex = "{";
            if (schema.has("properties") && schema.at("properties").isMappedObject()) {
                const Dictionary& props = schema.at("properties");
                bool first = true;
                for (auto const& p : props.items()) {
                    if (!first) ex += ", ";
                    first = false;
                    ex += "\"" + p.first + "\": ";
                    // recursively generate example for nested schema
                    if (p.second.isMappedObject() && p.second.has("type")) {
                        ex += generate_schema_example(p.second, schema_root, depth + 1);
                    } else if (p.second.has("$ref")) {
                        ex += generate_schema_example(p.second, schema_root, depth + 1);
                    } else {
                        ex += "...";
                    }
                }
            }
            ex += "}";
        } else if (t == "array") {
            ex = "[";
            if (schema.has("items")) {
                if (schema.at("items").isArrayObject()) {
                    ex += "<tuple>";
                } else {
                    ex += generate_schema_example(schema.at("items"), schema_root, depth + 1);
                }
            } else {
                ex += "...";
            }
            ex += "]";
        } else if (t == "string") {
            if (schema.has("enum") && schema.at("enum").isArrayObject()) {
                const Dictionary& en = schema.at("enum");
                if (en.size() > 0 && en[0].type() == Dictionary::String) {
                    ex = "\"" + en[0].asString() + "\"";
                    if (en.size() > 1) ex += " | ...";
                } else {
                    ex = "\"...\"";
                }
            } else {
                ex = "\"...\"";
            }
        } else if (t == "integer") {
            if (schema.has("minimum")) {
                ex = schema.at("minimum").dump();
            } else {
                ex = "0";
            }
        } else if (t == "number") {
            if (schema.has("minimum")) {
                ex = schema.at("minimum").dump();
            } else {
                ex = "0.0";
            }
        } else if (t == "boolean") {
            ex = "true";
        } else if (t == "null") {
            ex = "null";
        } else {
            ex = "<" + t + ">";
        }
    } else if (schema.has("enum") && schema.at("enum").isArrayObject()) {
        const Dictionary& en = schema.at("enum");
        if (en.size() > 0) {
            ex = en[0].dump();
            if (en.size() > 1) ex += " | ...";
        } else {
            ex = "<enum>";
        }
    } else if (schema.has("const")) {
        ex = schema.at("const").dump();
    } else {
        ex = "{...}";
    }

    return ex;
}

// Helper: convert internal path (dot/bracket style) to folder-style display path
static std::string display_path(const std::string& path) {
    if (path.empty()) return std::string("root");
    // replace dots with '/'
    std::string s = path;
    for (char& c : s)
        if (c == '.') c = '/';
    // convert [N] into /N
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '[') {
            size_t j = s.find(']', i);
            if (j != std::string::npos) {
                // append '/' then the index
                out.push_back('/');
                out.append(s.substr(i + 1, j - (i + 1)));
                i = j + 1;
                continue;
            }
        }
        out.push_back(s[i]);
        ++i;
    }
    // Remove trailing /0 if present
    if (out.size() >= 2 && out[out.size() - 2] == '/' && out[out.size() - 1] == '0') {
        out.resize(out.size() - 2);
    }
    return out;
}

static std::string value_type_name(const Dictionary& d) {
    if (d.isMappedObject()) return "object";
    if (d.isArrayObject()) return "array";
    switch (d.type()) {
        case Dictionary::String:
            return "string";
        case Dictionary::Integer:
            return "integer";
        case Dictionary::Double:
            return "number";
        case Dictionary::Boolean:
            return "boolean";
        case Dictionary::Null:
            return "null";
        default:
            return "unknown";
    }
}

static std::string value_preview(const Dictionary& d, size_t maxlen = 80) {
    std::string s = d.dump();
    if (s.size() > maxlen) s = s.substr(0, maxlen - 3) + "...";
    return s;
}

// Resolve a local JSON Pointer-style $ref (only supports local refs starting with "#/")
static const Dictionary* resolve_local_ref(const Dictionary& root, const std::string& ref) {
    if (ref.empty()) return nullptr;
    if (ref[0] != '#') return nullptr;  // only local refs supported for Phase 1
    // strip leading '#/' if present
    std::string path = ref;
    if (path.size() >= 2 && path[1] == '/')
        path = path.substr(2);
    else if (path.size() == 1)
        return &root;

    const Dictionary* cur = &root;
    size_t pos = 0;
    while (pos < path.size()) {
        size_t next = path.find('/', pos);
        std::string token =
                    (next == std::string::npos) ? path.substr(pos) : path.substr(pos, next - pos);
        pos = (next == std::string::npos) ? path.size() : next + 1;
        if (!cur->has(token)) return nullptr;
        const Dictionary& v = cur->at(token);
        if (!v.isMappedObject()) return nullptr;
        cur = &v;
    }
    return cur;
}

// Forward declarations of recursive validators
static std::optional<std::string> validate_node(const Dictionary& data,
                                                const Dictionary& schema_root,
                                                const Dictionary& schema_node,
                                                const std::string& path,
                                                const std::string& raw_content = "");

// Forward declaration for line number finding
static int find_line_number(const std::string& raw_content, const std::string& path);

// Helper: get a child schema dictionary from a Value that is expected to be an
// object. Some schema positions allow either a schema object or a string
// $ref (local JSON pointer). Accept the schema root so we can resolve local
// refs when a string is provided.
static const Dictionary* schema_from_value(const Dictionary& schema_root, const Dictionary& v) {
    if (v.isMappedObject()) return &v;
    // Accept a string as a local $ref (e.g. "#/definitions/foo")
    if (v.type() == Dictionary::String) return resolve_local_ref(schema_root, v.asString());
    return nullptr;
}

// Not needed: Dictionary represents objects directly.

// Validate a primitive numeric value against minimum/maximum/exclusive bounds in schema_node
static std::optional<std::string> check_numeric_constraints(const Dictionary& data,
                                                            const Dictionary& schema_node,
                                                            const std::string& path) {
    if (!(data.type() == Dictionary::Integer || data.type() == Dictionary::Double))
        return std::nullopt;
    double val = (data.type() == Dictionary::Integer) ? static_cast<double>(data.asInt())
                                                      : data.asDouble();
    // minimum
    if (schema_node.has("minimum")) {
        const Dictionary& minv = schema_node.at("minimum");
        if (minv.type() == Dictionary::Integer || minv.type() == Dictionary::Double) {
            double m = (minv.type() == Dictionary::Integer) ? static_cast<double>(minv.asInt())
                                                            : minv.asDouble();
            if (val < m)
                return std::optional<std::string>("property '" + path + "' value " +
                                                  std::to_string(val) + " below minimum " +
                                                  std::to_string(m));
        }
    }
    if (schema_node.has("exclusiveMinimum")) {
        const Dictionary& minv = schema_node.at("exclusiveMinimum");
        if (minv.type() == Dictionary::Integer || minv.type() == Dictionary::Double) {
            double m = (minv.type() == Dictionary::Integer) ? static_cast<double>(minv.asInt())
                                                            : minv.asDouble();
            if (val <= m)
                return std::optional<std::string>("property '" + path + "' value " +
                                                  std::to_string(val) + " <= exclusiveMinimum " +
                                                  std::to_string(m));
        }
    }
    if (schema_node.has("maximum")) {
        const Dictionary& maxv = schema_node.at("maximum");
        if (maxv.type() == Dictionary::Integer || maxv.type() == Dictionary::Double) {
            double M = (maxv.type() == Dictionary::Integer) ? static_cast<double>(maxv.asInt())
                                                            : maxv.asDouble();
            if (val > M)
                return std::optional<std::string>("property '" + path + "' value " +
                                                  std::to_string(val) + " above maximum " +
                                                  std::to_string(M));
        }
    }
    if (schema_node.has("exclusiveMaximum")) {
        const Dictionary& maxv = schema_node.at("exclusiveMaximum");
        if (maxv.type() == Dictionary::Integer || maxv.type() == Dictionary::Double) {
            double M = (maxv.type() == Dictionary::Integer) ? static_cast<double>(maxv.asInt())
                                                            : maxv.asDouble();
            if (val >= M)
                return std::optional<std::string>("property '" + path + "' value " +
                                                  std::to_string(val) + " >= exclusiveMaximum " +
                                                  std::to_string(M));
        }
    }
    return std::nullopt;
}

// Check enum keyword: schema_node.data["enum"] should be an array of literal values
static std::optional<std::string> check_enum(const Dictionary& data,
                                             const Dictionary& schema_node,
                                             const std::string& path) {
    if (!schema_node.has("enum")) return std::nullopt;
    const Dictionary& ev = schema_node.at("enum");
    if (!ev.isArrayObject()) return std::nullopt;

    if (std::getenv("PS_VALIDATE_DEBUG")) {
        std::cerr << "check_enum at path '" << path << "' data=" << data.dump() << "\n";
        std::cerr << "  enum options: ";
        for (int i = 0; i < ev.size(); ++i) {
            std::cerr << ev[i].dump() << " ";
        }
        std::cerr << "\n";
    }

    for (int i = 0; i < ev.size(); ++i) {
        if (ev[i] == data) {
            if (std::getenv("PS_VALIDATE_DEBUG")) {
                std::cerr << "  -> MATCHED option " << i << "\n";
            }
            return std::nullopt;
        }
    }

    if (std::getenv("PS_VALIDATE_DEBUG")) {
        std::cerr << "  -> NO MATCH, returning error\n";
    }

    // Build error message with actual value and list of valid options
    std::string msg = "'" + path + "' has value " + value_preview(data) + ".\n";
    msg += "But the valid options are:\n";

    // Collect enum values as strings for display and suggestions
    std::vector<std::string> enum_values;
    for (int i = 0; i < ev.size(); ++i) {
        std::string enum_val;
        if (ev[i].type() == Dictionary::String) {
            enum_val = ev[i].asString();
        } else {
            enum_val = ev[i].dump(0, true);
        }
        enum_values.push_back(enum_val);
        msg += "  - " + enum_val + "\n";
    }

    // Try to suggest the closest match if data is a string
    if (data.type() == Dictionary::String) {
        std::string data_str = data.asString();
        std::string data_lower = to_lower(data_str);
        int best_distance = std::numeric_limits<int>::max();
        std::string best_match;

        for (const auto& enum_val : enum_values) {
            // First check case-insensitive match (if same when lowercased, distance is 0)
            std::string enum_lower = to_lower(enum_val);
            if (data_lower == enum_lower) {
                best_distance = 0;
                best_match = enum_val;
                break;
            }

            // Otherwise compute regular distance
            int d = levenshtein_distance(data_str, enum_val);
            if (d < best_distance) {
                best_distance = d;
                best_match = enum_val;
            }
        }

        // Suggest if reasonably close (within 40% similarity or distance <= 3)
        // Also suggest if case-insensitive match (distance == 0 after case normalization)
        size_t maxlen = std::max(data_str.size(), best_match.size());
        double ratio =
                    maxlen == 0 ? 0.0
                                : static_cast<double>(best_distance) / static_cast<double>(maxlen);
        if (ratio <= 0.40 || best_distance <= 3 || to_lower(data_str) == to_lower(best_match)) {
            msg += "Did you mean '" + best_match + "'?";
        }
    }

    return std::optional<std::string>(msg);
}

static std::optional<std::string> validate_node(const Dictionary& data,
                                                const Dictionary& schema_root,
                                                const Dictionary& schema_node,
                                                const std::string& path,
                                                const std::string& raw_content) {
    // Minimal validator: primarily checks declared "type", numeric constraints and enum.
    if (std::getenv("PS_VALIDATE_DEBUG")) {
        std::cerr << "validate_node enter: path='" << path << "' data=" << data.dump()
                  << " schema_keys={";
        bool firstk = true;
        for (auto const& p : schema_node.items()) {
            if (!firstk) std::cerr << ",";
            firstk = false;
            std::cerr << p.first;
        }
        std::cerr << "}\n";
    }

    // resolve $ref if present
    if (schema_node.has("$ref") && schema_node.at("$ref").type() == Dictionary::String) {
        const std::string ref = schema_node.at("$ref").asString();
        const Dictionary* target = resolve_local_ref(schema_root, ref);
        if (!target) return std::optional<std::string>("unresolved $ref '" + ref + "' at " + path);
        return validate_node(data, schema_root, *target, path, raw_content);
    }

    // enum check
    if (schema_node.has("enum")) {
        if (auto e = check_enum(data, schema_node, path)) return e;
    }

    // const keyword: value must equal the provided literal
    if (schema_node.has("const")) {
        if (!(schema_node.at("const") == data)) {
            return std::optional<std::string>("key '" + path + "' does not match const value");
        }
    }

    // allOf/anyOf/oneOf - basic handling
    if (schema_node.has("allOf") && schema_node.at("allOf").isArrayObject()) {
        const Dictionary& arr = schema_node.at("allOf");
        for (int i = 0; i < arr.size(); ++i) {
            const Dictionary& sub = arr[i];
            const Dictionary* subSchema = schema_from_value(schema_root, sub);
            if (!subSchema) continue;
            if (auto err = validate_node(data, schema_root, *subSchema, path, raw_content))
                return err;
        }
    }
    // anyOf
    if (schema_node.has("anyOf") && schema_node.at("anyOf").isArrayObject()) {
        const Dictionary& arr = schema_node.at("anyOf");
        bool matched = false;
        const Dictionary* deprecated_matched_schema = nullptr;
        std::vector<std::string> failures;
        for (int i = 0; i < arr.size(); ++i) {
            const Dictionary& sub = arr[i];
            const Dictionary* subSchema = schema_from_value(schema_root, sub);
            if (!subSchema) continue;
            auto err = validate_node(data, schema_root, *subSchema, path, raw_content);
            if (!err.has_value()) {
                // This alternative matched
                matched = true;

                // Check if this matched alternative is deprecated
                const Dictionary* resolved = subSchema;
                if (subSchema->has("$ref") && subSchema->at("$ref").type() == Dictionary::String) {
                    resolved = resolve_local_ref(schema_root, subSchema->at("$ref").asString());
                    if (!resolved) resolved = subSchema;
                }

                if (resolved->has("deprecated") &&
                    resolved->at("deprecated").type() == Dictionary::Boolean &&
                    resolved->at("deprecated").asBool()) {
                    // Found a deprecated match - save it
                    deprecated_matched_schema = resolved;
                }

                // Continue checking other alternatives to find deprecated matches
            } else {
                std::string schemaName = extract_schema_name(*subSchema, schema_root);
                failures.push_back("Option: " + schemaName + "\n  Doesn't match because: " + *err);
            }
        }
        if (!matched) {
            std::string msg = "anyOf did not match any schema at '" + display_path(path) + "'";
            // format with ron parser
            msg += std::string("\n Your value: ") + ps::dump_ron(data);
            if (!failures.empty()) {
                msg += "\n\nAlternatives:";
                for (const auto& f : failures) {
                    msg += "\n" + f + "\n";
                }
            }
            return std::optional<std::string>(msg);
        }

        // Check if any deprecated alternative matched
        if (matched && deprecated_matched_schema != nullptr) {
            std::string msg = "Using deprecated option";
            if (deprecated_matched_schema->has("const")) {
                msg = "Value '" + deprecated_matched_schema->at("const").dump() + "' is deprecated";
            }
            if (deprecated_matched_schema->has("description") &&
                deprecated_matched_schema->at("description").type() == Dictionary::String) {
                msg += ": " + deprecated_matched_schema->at("description").asString();
            }
            return std::optional<std::string>(msg);
        }
    }

    // oneOf
    if (schema_node.has("oneOf") && schema_node.at("oneOf").isArrayObject()) {
        const Dictionary& arr = schema_node.at("oneOf");
        int matches = 0;
        std::vector<std::string> failures;
        std::vector<int> matched_indices;
        std::vector<const Dictionary*> matched_schemas;
        for (int i = 0; i < arr.size(); ++i) {
            const Dictionary& sub = arr[i];
            const Dictionary* subSchema = schema_from_value(schema_root, sub);
            if (!subSchema) continue;
            auto err = validate_node(data, schema_root, *subSchema, path, raw_content);
            if (!err.has_value()) {
                ++matches;
                matched_indices.push_back(i);
                matched_schemas.push_back(subSchema);
            } else {
                std::string schemaName = extract_schema_name(*subSchema, schema_root);
                failures.push_back("Option: " + schemaName + "\n  Failed because: " + *err);
            }
        }
        if (matches != 1) {
            if (matches == 0) {
                std::string msg = "oneOf did not match any schema at '" + display_path(path) + "'";
                msg += std::string("\n Your value: ") + ps::dump_ron(data);
                if (!failures.empty()) {
                    msg += "\n\nAlternatives:";
                    for (const auto& f : failures) {
                        msg += "\n" + f + "\n";
                    }
                }
                return std::optional<std::string>(msg);
            } else {
                std::string msg = "oneOf matched multiple schemas (" + std::to_string(matches) +
                                  ") at '" + display_path(path) + "'";
                msg += std::string("\n Your value: ") + ps::dump_ron(data);
                msg += "\n  Matched alternatives:";
                for (int idx : matched_indices) {
                    msg += " " + std::to_string(idx);
                }
                return std::optional<std::string>(msg);
            }
        }

        // Check if any of the matched alternatives are deprecated
        if (matches >= 1) {
            for (const Dictionary* matched_schema : matched_schemas) {
                // Resolve $ref if present
                const Dictionary* resolved_schema = matched_schema;
                if (matched_schema->has("$ref") &&
                    matched_schema->at("$ref").type() == Dictionary::String) {
                    resolved_schema = resolve_local_ref(schema_root,
                                                        matched_schema->at("$ref").asString());
                    if (!resolved_schema) resolved_schema = matched_schema;
                }

                if (resolved_schema->has("deprecated") &&
                    resolved_schema->at("deprecated").type() == Dictionary::Boolean &&
                    resolved_schema->at("deprecated").asBool()) {
                    std::string msg = "Using deprecated option";
                    if (resolved_schema->has("const")) {
                        msg = "Value '" + resolved_schema->at("const").dump() + "' is deprecated";
                    }
                    if (resolved_schema->has("description") &&
                        resolved_schema->at("description").type() == Dictionary::String) {
                        msg += ": " + resolved_schema->at("description").asString();
                    }
                    return std::optional<std::string>(msg);
                }
            }
        }
    }

    // type check
    // Infer object type if schema has "properties" even without explicit "type": "object"
    bool should_validate_as_object = false;
    if (schema_node.has("type") && schema_node.at("type").type() == Dictionary::String) {
        std::string t = schema_node.at("type").asString();
        if (t == "object") {
            should_validate_as_object = true;
        }
    } else if (data.isMappedObject() &&
               (schema_node.has("properties") || schema_node.has("required") ||
                schema_node.has("additionalProperties") || schema_node.has("patternProperties"))) {
        // Schema has object-specific fields but no type - infer it's meant for objects
        should_validate_as_object = true;
    }

    if (should_validate_as_object) {
        // Validate mapped object: properties, required, additionalProperties,
        // patternProperties, minProperties/maxProperties
        if (!data.isMappedObject())
            return std::optional<std::string>(limit_line_length(
                        "expected type 'object' at '" + display_path(path) + "' but found '" +
                        value_type_name(data) + "' (value: " + value_preview(data) + ")"));

        // gather property schemas
        const Dictionary* properties = nullptr;
        if (schema_node.has("properties") && schema_node.at("properties").isMappedObject()) {
            properties = &schema_node.at("properties");
        }

        // patternProperties
        const Dictionary* patternProps = nullptr;
        if (schema_node.has("patternProperties") &&
            schema_node.at("patternProperties").isMappedObject()) {
            patternProps = &schema_node.at("patternProperties");
        }

        // additionalProperties may be boolean or schema
        const Dictionary* it_add = nullptr;
        if (schema_node.has("additionalProperties"))
            it_add = &schema_node.at("additionalProperties");

        // required can be specified in two styles: per-property boolean or parent-level array
        std::set<std::string> required_names;
        if (schema_node.has("required")) {
            const Dictionary& r = schema_node.at("required");
            // support string array or object array containing strings
            try {
                auto svec = r.asStrings();
                for (auto const& s : svec) required_names.insert(s);
            } catch (...) {
                if (r.isArrayObject()) {
                    for (int i = 0; i < r.size(); ++i) {
                        const Dictionary& e = r[i];
                        if (e.type() == Dictionary::String) required_names.insert(e.asString());
                    }
                }
            }
        }

        // Enforce parent-level required names even if no explicit "properties" are listed
        for (auto const& rn : required_names) {
            if (!data.has(rn)) {
                // Before reporting as missing, check if a deprecated alternative exists
                // that should be validated instead
                bool found_deprecated_alternative = false;
                if (properties) {
                    for (auto const& dprop : data.items()) {
                        const std::string& candidate = dprop.first;
                        // Check if this key is in properties and marked as deprecated
                        if (properties->has(candidate)) {
                            const Dictionary& prop_schema = properties->at(candidate);
                            if (prop_schema.has("deprecated") &&
                                prop_schema.at("deprecated").type() == Dictionary::Boolean &&
                                prop_schema.at("deprecated").asBool()) {
                                // Found a deprecated alternative - it will be validated below
                                found_deprecated_alternative = true;
                                break;
                            }
                        }
                    }
                }

                if (found_deprecated_alternative) {
                    // Don't report as missing - the deprecated alternative will be
                    // validated and flagged in the property iteration below
                    continue;
                }

                // Check if there's a similar key in the data that might be a typo of the
                // required name. Only suggest keys that are NOT already allowed by the schema.
                std::string suggestion;
                for (auto const& dprop : data.items()) {
                    const std::string& candidate = dprop.first;
                    // Skip if this key is explicitly allowed in properties
                    if (properties && properties->has(candidate)) continue;

                    int d = levenshtein_distance(candidate, rn);
                    size_t maxlen = std::max(candidate.size(), rn.size());
                    double ratio =
                                maxlen == 0 ? 0.0
                                            : static_cast<double>(d) / static_cast<double>(maxlen);
                    if (ratio <= 0.40 || d <= 2) {
                        suggestion = candidate;
                        break;
                    }
                }

                if (!suggestion.empty()) {
                    std::string msg = "key '" + suggestion + "' not allowed";
                    msg += " Did you mean '" + rn + "'?";
                    return std::optional<std::string>(msg);
                }
                std::string full_key = path.empty() ? rn : path + "." + rn;
                std::string msg = "missing required key '" + full_key + "'";
                // Add line number context if available
                if (!raw_content.empty()) {
                    int line = -1;
                    if (!path.empty()) {
                        // For nested objects, find the parent object
                        line = find_line_number(raw_content, path);
                    } else if (data.size() > 0) {
                        // For root level, find the first existing key for context
                        for (auto const& item : data.items()) {
                            line = find_line_number(raw_content, item.first);
                            if (line > 0) break;
                        }
                    }
                    if (line > 0) {
                        msg = "line " + std::to_string(line) + ": " + msg;
                    }
                }
                return std::optional<std::string>(msg);
            }
        }

        // minProperties/maxProperties
        if (schema_node.has("minProperties") &&
            schema_node.at("minProperties").type() == Dictionary::Integer) {
            if (data.size() < schema_node.at("minProperties").asInt())
                return std::optional<std::string>("object has fewer properties than minProperties");
        }
        if (schema_node.has("maxProperties") &&
            schema_node.at("maxProperties").type() == Dictionary::Integer) {
            if (data.size() > schema_node.at("maxProperties").asInt())
                return std::optional<std::string>("object has more properties than maxProperties");
        }

        // iterate expected properties
        if (properties) {
            for (auto const& p : properties->items()) {
                const std::string& key = p.first;
                const Dictionary& propSchema = p.second;
                bool has = data.has(key);
                // per-property required as boolean
                auto itreq = propSchema.has("required") ? &propSchema.at("required") : nullptr;
                if (!has) {
                    std::string full_key = path.empty() ? key : path + "." + key;
                    std::string msg = "missing required key '" + full_key + "'";
                    // Add line number context if available
                    if (!raw_content.empty()) {
                        int line = -1;
                        if (!path.empty()) {
                            // For nested objects, find the parent object
                            line = find_line_number(raw_content, path);
                        } else if (data.size() > 0) {
                            // For root level, find the first existing key for context
                            for (auto const& item : data.items()) {
                                line = find_line_number(raw_content, item.first);
                                if (line > 0) break;
                            }
                        }
                        if (line > 0) {
                            msg = "line " + std::to_string(line) + ": " + msg;
                        }
                    }
                    if (itreq != nullptr && itreq->type() == Dictionary::Boolean && itreq->asBool())
                        return std::optional<std::string>(msg);
                    if (required_names.find(key) != required_names.end())
                        return std::optional<std::string>(msg);
                    continue;
                }
                // validate present property
                const Dictionary& subSchemaValue = propSchema;
                const Dictionary* subSchema = schema_from_value(schema_root, subSchemaValue);
                if (subSchema) {
                    if (auto err = validate_node(data.at(key),
                                                 schema_root,
                                                 *subSchema,
                                                 path.empty() ? key : path + "." + key,
                                                 raw_content))
                        return err;

                    // Check if property is deprecated
                    if (propSchema.has("deprecated") &&
                        propSchema.at("deprecated").type() == Dictionary::Boolean &&
                        propSchema.at("deprecated").asBool()) {
                        std::string full_key = path.empty() ? key : path + "." + key;
                        std::string msg = "Property '" + full_key + "' is deprecated";
                        if (propSchema.has("description") &&
                            propSchema.at("description").type() == Dictionary::String) {
                            msg += ": " + propSchema.at("description").asString();
                        }
                        // Add line number if raw_content is available
                        if (!raw_content.empty()) {
                            int line = find_line_number(raw_content, full_key);
                            if (line > 0) {
                                msg = "line " + std::to_string(line) + ": " + msg;
                            }
                        }
                        return std::optional<std::string>(msg);
                    }
                }
            }
        }

        // now check each data property for patternProperties/additionalProperties
        for (auto const& dprop : data.items()) {
            const std::string& key = dprop.first;
            bool handled = false;
            if (properties && properties->has(key)) {
                handled = true;
            }
            // check patternProperties
            if (!handled && patternProps) {
                for (auto const& pp : patternProps->items()) {
                    try {
                        std::regex rx(pp.first);
                        if (std::regex_match(key, rx)) {
                            const Dictionary* sub = schema_from_value(schema_root, pp.second);
                            if (sub) {
                                if (auto err = validate_node(data.at(key),
                                                             schema_root,
                                                             *sub,
                                                             path.empty() ? key : path + "." + key,
                                                             raw_content))
                                    return err;
                            }
                            handled = true;
                            break;
                        }
                    } catch (...) {
                        // invalid regex - ignore
                    }
                }
            }
            if (handled) continue;

            // additionalProperties
            if (it_add == nullptr) {
                // allowed by default
                continue;
            }
            const Dictionary& ap = *it_add;
            if (ap.type() == Dictionary::Boolean) {
                if (!ap.asBool()) {
                    // Try to suggest nearby property names from the declared properties
                    std::vector<std::string> suggestions = find_nearby_keys(key, properties);
                    std::string msg = "key '" + key + "' not valid";
                    if (!path.empty()) {
                        msg += " in '" + path + "'";
                    }
                    msg += ".";
                    if (!suggestions.empty()) {
                        msg += "\nDid you mean ";
                        if (suggestions.size() == 1) {
                            msg += "'" + suggestions[0] + "'?";
                        } else {
                            for (size_t si = 0; si < suggestions.size(); ++si) {
                                if (si > 0) {
                                    if (si + 1 == suggestions.size())
                                        msg += " or ";
                                    else
                                        msg += ", ";
                                }
                                msg += "'" + suggestions[si] + "'";
                            }
                            msg += "?";
                        }
                    }
                    return std::optional<std::string>(msg);
                }
            } else {
                const Dictionary* sub = schema_from_value(schema_root, ap);
                if (sub) {
                    if (auto err = validate_node(data.at(key),
                                                 schema_root,
                                                 *sub,
                                                 path.empty() ? key : path + "." + key,
                                                 raw_content))
                        return err;
                }
            }
        }

        return std::nullopt;
    }

    // Continue with type checking for other types
    if (schema_node.has("type") && schema_node.at("type").type() == Dictionary::String) {
        std::string t = schema_node.at("type").asString();
        if (t == "array") {
            if (!data.isArrayObject())
                return std::optional<std::string>(limit_line_length(
                            "expected type 'array' at '" + display_path(path) + "' but found '" +
                            value_type_name(data) + "' (value: " + value_preview(data) + ")"));

            // minItems / maxItems
            if (schema_node.has("minItems") &&
                schema_node.at("minItems").type() == Dictionary::Integer) {
                if (data.size() < schema_node.at("minItems").asInt())
                    return std::optional<std::string>("array too few items");
            }
            if (schema_node.has("maxItems") &&
                schema_node.at("maxItems").type() == Dictionary::Integer) {
                if (data.size() > schema_node.at("maxItems").asInt())
                    return std::optional<std::string>("array too many items");
            }

            // uniqueItems
            if (schema_node.has("uniqueItems") &&
                schema_node.at("uniqueItems").type() == Dictionary::Boolean &&
                schema_node.at("uniqueItems").asBool()) {
                std::set<std::string> seen;
                for (int i = 0; i < data.size(); ++i) {
                    const Dictionary& el = data[i];
                    std::string key = el.dump();
                    if (seen.find(key) != seen.end())
                        return std::optional<std::string>("array has duplicate items");
                    seen.insert(key);
                }
            }

            // items: schema or tuple
            if (schema_node.has("items")) {
                const Dictionary& itemsVal = schema_node.at("items");
                if (itemsVal.isArrayObject()) {
                    // tuple validation
                    size_t nSchemas = itemsVal.size();
                    const Dictionary* itadd = nullptr;
                    if (schema_node.has("additionalItems"))
                        itadd = &schema_node.at("additionalItems");
                    for (int i = 0; i < data.size(); ++i) {
                        if (static_cast<size_t>(i) < nSchemas) {
                            const Dictionary* sub = schema_from_value(schema_root, itemsVal[i]);
                            if (sub) {
                                if (auto err = validate_node(data[i],
                                                             schema_root,
                                                             *sub,
                                                             path + "[" + std::to_string(i) + "]",
                                                             raw_content))
                                    return err;
                            }
                        } else {
                            // additionalItems handling
                            if (itadd != nullptr) {
                                if (itadd->type() == Dictionary::Boolean) {
                                    if (!itadd->asBool())
                                        return std::optional<std::string>(
                                                    "additional tuple items not allowed");
                                } else {
                                    const Dictionary* sub = schema_from_value(schema_root, *itadd);
                                    if (sub) {
                                        if (auto err = validate_node(
                                                        data[i],
                                                        schema_root,
                                                        *sub,
                                                        path + "[" + std::to_string(i) + "]",
                                                        raw_content))
                                            return err;
                                    }
                                }
                            }
                        }
                    }
                } else {
                    const Dictionary* sub = schema_from_value(schema_root, itemsVal);
                    if (sub) {
                        for (int i = 0; i < data.size(); ++i) {
                            if (auto err = validate_node(data[i],
                                                         schema_root,
                                                         *sub,
                                                         path + "[" + std::to_string(i) + "]",
                                                         raw_content))
                                return err;
                        }
                    }
                }
            }

            return std::nullopt;
        }
        if (t == "string") {
            if (data.type() != Dictionary::String)
                return std::optional<std::string>(limit_line_length(
                            "expected type 'string' at '" + display_path(path) + "' but found '" +
                            value_type_name(data) + "' (value: " + value_preview(data) + ")"));
            // minLength / maxLength
            if (schema_node.has("minLength") &&
                schema_node.at("minLength").type() == Dictionary::Integer) {
                if ((int)data.asString().size() < schema_node.at("minLength").asInt())
                    return std::optional<std::string>("string shorter than minLength");
            }
            if (schema_node.has("maxLength") &&
                schema_node.at("maxLength").type() == Dictionary::Integer) {
                if ((int)data.asString().size() > schema_node.at("maxLength").asInt())
                    return std::optional<std::string>("string longer than maxLength");
            }
            // pattern
            if (schema_node.has("pattern") &&
                schema_node.at("pattern").type() == Dictionary::String) {
                try {
                    std::regex rx(schema_node.at("pattern").asString());
                    if (!std::regex_match(data.asString(), rx)) {
                        return std::optional<std::string>("string does not match pattern");
                    }
                } catch (...) {
                    // invalid regex -> skip
                }
            }
            return std::nullopt;
        }
        if (t == "integer") {
            if (data.type() != Dictionary::Integer)
                return std::optional<std::string>(limit_line_length(
                            "expected type 'integer' at '" + display_path(path) + "' but found '" +
                            value_type_name(data) + "' (value: " + value_preview(data) + ")"));
            if (auto e = check_numeric_constraints(data, schema_node, path)) return e;
            return std::nullopt;
        }
        if (t == "number") {
            if (!(data.type() == Dictionary::Double || data.type() == Dictionary::Integer))
                return std::optional<std::string>(limit_line_length(
                            "expected type 'number' at '" + display_path(path) + "' but found '" +
                            value_type_name(data) + "' (value: " + value_preview(data) + ")"));
            if (auto e = check_numeric_constraints(data, schema_node, path)) return e;
            return std::nullopt;
        }
        if (t == "boolean") {
            if (data.type() != Dictionary::Boolean)
                return std::optional<std::string>(limit_line_length(
                            "expected type 'boolean' at '" + display_path(path) + "' but found '" +
                            value_type_name(data) + "' (value: " + value_preview(data) + ")"));
            return std::nullopt;
        }
    }

    // fallback: apply numeric/enum constraints if present
    if (auto e = check_enum(data, schema_node, path)) return e;
    if (auto e2 = check_numeric_constraints(data, schema_node, path)) return e2;

    return std::nullopt;
}

std::optional<std::string> validate(const Dictionary& data, const Dictionary& schema) {
    if (std::getenv("PS_VALIDATE_DEBUG")) {
        std::cerr << "validate debug: data=" << data.dump() << " schema=" << schema.dump() << "\n";
    }
    // Support a convenience form where the top-level schema is a map of
    // property-name -> property-schema (without an explicit { "type": "object" }).
    // Only apply this when the supplied schema does not itself look like a
    // schema object (i.e. does not contain known schema keywords).
    const std::set<std::string> schema_keys = {"type",
                                               "properties",
                                               "items",
                                               "additionalProperties",
                                               "patternProperties",
                                               "required",
                                               "enum",
                                               "allOf",
                                               "anyOf",
                                               "oneOf",
                                               "minItems",
                                               "maxItems",
                                               "minProperties",
                                               "maxProperties",
                                               "uniqueItems"};
    bool contains_schema_keyword = false;
    if (schema.isMappedObject()) {
        for (auto const& p : schema.items()) {
            if (schema_keys.find(p.first) != schema_keys.end()) {
                contains_schema_keyword = true;
                break;
            }
        }
    }
    if (schema.isMappedObject() && !contains_schema_keyword && !schema.has("properties")) {
        Dictionary wrapper;
        wrapper["type"] = std::string("object");
        wrapper["properties"] = schema;
        return validate_node(data, schema, wrapper, "");
    }
    return validate_node(data, schema, schema, "");
}

// Helper: find line number for a key in raw JSON/RON content
// Searches for the key at the specified path (e.g., "mesh adaptation.spalding yplus")
static int find_line_number(const std::string& raw_content, const std::string& path) {
    if (path.empty()) return -1;

    // Split path into components
    std::vector<std::string> components;
    std::string current;
    for (char c : path) {
        if (c == '.') {
            if (!current.empty()) {
                components.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        components.push_back(current);
    }

    if (components.empty()) return -1;

    // Search for the last component (the actual key with the error)
    std::string search_key = components.back();

    // Count lines and search for the key in quotes
    int line = 1;
    size_t pos = 0;
    int best_line = -1;

    // Look for the key as a quoted string followed by colon
    std::string pattern1 = "\"" + search_key + "\"";
    std::string pattern2 = "'" + search_key + "'";

    while (pos < raw_content.size()) {
        // Check if we found the key at this position
        if (raw_content.compare(pos, pattern1.size(), pattern1) == 0 ||
            raw_content.compare(pos, pattern2.size(), pattern2) == 0) {
            // Check if followed by : (ignoring whitespace)
            size_t colon_pos = pos + pattern1.size();
            while (colon_pos < raw_content.size() &&
                   (raw_content[colon_pos] == ' ' || raw_content[colon_pos] == '\t')) {
                colon_pos++;
            }
            if (colon_pos < raw_content.size() && raw_content[colon_pos] == ':') {
                best_line = line;
            }
        }

        // Count newlines
        if (raw_content[pos] == '\n') {
            line++;
        }
        pos++;
    }

    return best_line;
}

// Validate with line number tracking
std::optional<std::string> validate(const Dictionary& data,
                                    const Dictionary& schema,
                                    const std::string& raw_content) {
    // Call validate_node directly with raw_content instead of post-processing
    std::optional<std::string> result;

    // Support convenience form (same logic as 2-parameter validate)
    const std::set<std::string> schema_keys = {"type",
                                               "properties",
                                               "items",
                                               "additionalProperties",
                                               "patternProperties",
                                               "required",
                                               "enum",
                                               "allOf",
                                               "anyOf",
                                               "oneOf",
                                               "minItems",
                                               "maxItems",
                                               "minProperties",
                                               "maxProperties",
                                               "uniqueItems"};
    bool contains_schema_keyword = false;
    if (schema.isMappedObject()) {
        for (auto const& p : schema.items()) {
            if (schema_keys.find(p.first) != schema_keys.end()) {
                contains_schema_keyword = true;
                break;
            }
        }
    }
    if (schema.isMappedObject() && !contains_schema_keyword && !schema.has("properties")) {
        Dictionary wrapper;
        wrapper["type"] = std::string("object");
        wrapper["properties"] = schema;
        result = validate_node(data, schema, wrapper, "", raw_content);
    } else {
        result = validate_node(data, schema, schema, "", raw_content);
    }
    return result;
}

}  // namespace ps
