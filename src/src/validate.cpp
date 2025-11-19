// All helpers and code inside a single ps namespace
#include <string>
#include "ps/validate.h"
#include <sstream>
#include <vector>
#include <algorithm>
#include <regex>
#include <set>
#include <cstdlib>
#include <iostream>

namespace ps {
// Helper: limit error message to 80 characters (truncate with ...)
static std::string limit_line_length(const std::string& msg, size_t maxlen = 80) {
    if (msg.size() <= maxlen) return msg;
    return msg.substr(0, maxlen - 3) + "...";
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
        std::string token = (next == std::string::npos) ? path.substr(pos) : path.substr(pos, next - pos);
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
                                                const std::string& path);

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
    if (!(data.type() == Dictionary::Integer || data.type() == Dictionary::Double)) return std::nullopt;
    double val = (data.type() == Dictionary::Integer) ? static_cast<double>(data.asInt()) : data.asDouble();
    // minimum
    if (schema_node.has("minimum")) {
        const Dictionary& minv = schema_node.at("minimum");
        if (minv.type() == Dictionary::Integer || minv.type() == Dictionary::Double) {
            double m = (minv.type() == Dictionary::Integer) ? static_cast<double>(minv.asInt()) : minv.asDouble();
            if (val < m)
                return std::optional<std::string>("property '" + path + "' value " + std::to_string(val) +
                                                  " below minimum " + std::to_string(m));
        }
    }
    if (schema_node.has("exclusiveMinimum")) {
        const Dictionary& minv = schema_node.at("exclusiveMinimum");
        if (minv.type() == Dictionary::Integer || minv.type() == Dictionary::Double) {
            double m = (minv.type() == Dictionary::Integer) ? static_cast<double>(minv.asInt()) : minv.asDouble();
            if (val <= m)
                return std::optional<std::string>("property '" + path + "' value " + std::to_string(val) +
                                                  " <= exclusiveMinimum " + std::to_string(m));
        }
    }
    if (schema_node.has("maximum")) {
        const Dictionary& maxv = schema_node.at("maximum");
        if (maxv.type() == Dictionary::Integer || maxv.type() == Dictionary::Double) {
            double M = (maxv.type() == Dictionary::Integer) ? static_cast<double>(maxv.asInt()) : maxv.asDouble();
            if (val > M)
                return std::optional<std::string>("property '" + path + "' value " + std::to_string(val) +
                                                  " above maximum " + std::to_string(M));
        }
    }
    if (schema_node.has("exclusiveMaximum")) {
        const Dictionary& maxv = schema_node.at("exclusiveMaximum");
        if (maxv.type() == Dictionary::Integer || maxv.type() == Dictionary::Double) {
            double M = (maxv.type() == Dictionary::Integer) ? static_cast<double>(maxv.asInt()) : maxv.asDouble();
            if (val >= M)
                return std::optional<std::string>("property '" + path + "' value " + std::to_string(val) +
                                                  " >= exclusiveMaximum " + std::to_string(M));
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
    for (int i = 0; i < ev.size(); ++i) {
        if (ev[i] == data) return std::nullopt;
    }
    return std::optional<std::string>("property '" + path + "' not one of enum values");
}

static std::optional<std::string> validate_node(const Dictionary& data,
                                                const Dictionary& schema_root,
                                                const Dictionary& schema_node,
                                                const std::string& path) {
    // Minimal validator: primarily checks declared "type", numeric constraints and enum.
    if (std::getenv("PS_VALIDATE_DEBUG")) {
        std::cerr << "validate_node enter: path='" << path << "' data=" << data.dump() << " schema_keys={";
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
        return validate_node(data, schema_root, *target, path);
    }

    // enum check
    if (schema_node.has("enum")) {
        if (auto e = check_enum(data, schema_node, path)) return e;
    }

    // const keyword: value must equal the provided literal
    if (schema_node.has("const")) {
        if (!(schema_node.at("const") == data)) {
            return std::optional<std::string>("property '" + path + "' does not match const value");
        }
    }

    // allOf/anyOf/oneOf - basic handling
    if (schema_node.has("allOf") && schema_node.at("allOf").isArrayObject()) {
        const Dictionary& arr = schema_node.at("allOf");
        for (int i = 0; i < arr.size(); ++i) {
            const Dictionary& sub = arr[i];
            const Dictionary* subSchema = schema_from_value(schema_root, sub);
            if (!subSchema) continue;
            if (auto err = validate_node(data, schema_root, *subSchema, path)) return err;
        }
    }
    // anyOf
    if (schema_node.has("anyOf") && schema_node.at("anyOf").isArrayObject()) {
        const Dictionary& arr = schema_node.at("anyOf");
        bool matched = false;
        for (int i = 0; i < arr.size(); ++i) {
            const Dictionary& sub = arr[i];
            const Dictionary* subSchema = schema_from_value(schema_root, sub);
            if (!subSchema) continue;
            if (!validate_node(data, schema_root, *subSchema, path).has_value()) {
                matched = true;
                break;
            }
        }
        if (!matched) return std::optional<std::string>("no alternatives in anyOf matched");
    }

    // oneOf
    if (schema_node.has("oneOf") && schema_node.at("oneOf").isArrayObject()) {
        const Dictionary& arr = schema_node.at("oneOf");
        int matches = 0;
        for (int i = 0; i < arr.size(); ++i) {
            const Dictionary& sub = arr[i];
            const Dictionary* subSchema = schema_from_value(schema_root, sub);
            if (!subSchema) continue;
            if (!validate_node(data, schema_root, *subSchema, path).has_value()) {
                ++matches;
            }
        }
        if (matches != 1) return std::optional<std::string>("oneOf did not match exactly one schema");
    }

    // type check
    if (schema_node.has("type") && schema_node.at("type").type() == Dictionary::String) {
        std::string t = schema_node.at("type").asString();
        if (t == "object") {
            // Validate mapped object: properties, required, additionalProperties,
            // patternProperties, minProperties/maxProperties
            if (!data.isMappedObject())
                return std::optional<std::string>(limit_line_length("expected type 'object' at '" + display_path(path) +
                                                                    "' but found '" + value_type_name(data) +
                                                                    "' (value: " + value_preview(data) + ")"));

            // gather property schemas
            const Dictionary* properties = nullptr;
            if (schema_node.has("properties") && schema_node.at("properties").isMappedObject()) {
                properties = &schema_node.at("properties");
            }

            // patternProperties
            const Dictionary* patternProps = nullptr;
            if (schema_node.has("patternProperties") && schema_node.at("patternProperties").isMappedObject()) {
                patternProps = &schema_node.at("patternProperties");
            }

            // additionalProperties may be boolean or schema
            const Dictionary* it_add = nullptr;
            if (schema_node.has("additionalProperties")) it_add = &schema_node.at("additionalProperties");

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
                    return std::optional<std::string>("missing required property '" + rn + "'");
                }
            }

            // minProperties/maxProperties
            if (schema_node.has("minProperties") && schema_node.at("minProperties").type() == Dictionary::Integer) {
                if (data.size() < schema_node.at("minProperties").asInt())
                    return std::optional<std::string>("object has fewer properties than minProperties");
            }
            if (schema_node.has("maxProperties") && schema_node.at("maxProperties").type() == Dictionary::Integer) {
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
                        if (itreq != nullptr && itreq->type() == Dictionary::Boolean && itreq->asBool())
                            return std::optional<std::string>("missing required property '" + key + "'");
                        if (required_names.find(key) != required_names.end())
                            return std::optional<std::string>("missing required property '" + key + "'");
                        continue;
                    }
                    // validate present property
                    const Dictionary& subSchemaValue = propSchema;
                    const Dictionary* subSchema = schema_from_value(schema_root, subSchemaValue);
                    if (subSchema) {
                        if (auto err = validate_node(
                                data.at(key), schema_root, *subSchema, path.empty() ? key : path + "." + key))
                            return err;
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
                                    if (auto err = validate_node(
                                            data.at(key), schema_root, *sub, path.empty() ? key : path + "." + key))
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
                        return std::optional<std::string>("property '" + key + "' not allowed");
                    }
                } else {
                    const Dictionary* sub = schema_from_value(schema_root, ap);
                    if (sub) {
                        if (auto err =
                                validate_node(data.at(key), schema_root, *sub, path.empty() ? key : path + "." + key))
                            return err;
                    }
                }
            }

            return std::nullopt;
        } else if (t == "array") {
            if (!data.isArrayObject())
                return std::optional<std::string>(limit_line_length("expected type 'array' at '" + display_path(path) +
                                                                    "' but found '" + value_type_name(data) +
                                                                    "' (value: " + value_preview(data) + ")"));

            // minItems / maxItems
            if (schema_node.has("minItems") && schema_node.at("minItems").type() == Dictionary::Integer) {
                if (data.size() < schema_node.at("minItems").asInt())
                    return std::optional<std::string>("array too few items");
            }
            if (schema_node.has("maxItems") && schema_node.at("maxItems").type() == Dictionary::Integer) {
                if (data.size() > schema_node.at("maxItems").asInt())
                    return std::optional<std::string>("array too many items");
            }

            // uniqueItems
            if (schema_node.has("uniqueItems") && schema_node.at("uniqueItems").type() == Dictionary::Boolean &&
                schema_node.at("uniqueItems").asBool()) {
                std::set<std::string> seen;
                for (int i = 0; i < data.size(); ++i) {
                    const Dictionary& el = data[i];
                    std::string key = el.dump();
                    if (seen.find(key) != seen.end()) return std::optional<std::string>("array has duplicate items");
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
                    if (schema_node.has("additionalItems")) itadd = &schema_node.at("additionalItems");
                    for (int i = 0; i < data.size(); ++i) {
                        if (static_cast<size_t>(i) < nSchemas) {
                            const Dictionary* sub = schema_from_value(schema_root, itemsVal[i]);
                            if (sub) {
                                if (auto err =
                                        validate_node(data[i], schema_root, *sub, path + "[" + std::to_string(i) + "]"))
                                    return err;
                            }
                        } else {
                            // additionalItems handling
                            if (itadd != nullptr) {
                                if (itadd->type() == Dictionary::Boolean) {
                                    if (!itadd->asBool())
                                        return std::optional<std::string>("additional tuple items not allowed");
                                } else {
                                    const Dictionary* sub = schema_from_value(schema_root, *itadd);
                                    if (sub) {
                                        if (auto err = validate_node(
                                                data[i], schema_root, *sub, path + "[" + std::to_string(i) + "]"))
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
                            if (auto err =
                                    validate_node(data[i], schema_root, *sub, path + "[" + std::to_string(i) + "]"))
                                return err;
                        }
                    }
                }
            }

            return std::nullopt;
        } else if (t == "string") {
            if (data.type() != Dictionary::String)
                return std::optional<std::string>(limit_line_length("expected type 'string' at '" + display_path(path) +
                                                                    "' but found '" + value_type_name(data) +
                                                                    "' (value: " + value_preview(data) + ")"));
            // minLength / maxLength
            if (schema_node.has("minLength") && schema_node.at("minLength").type() == Dictionary::Integer) {
                if ((int)data.asString().size() < schema_node.at("minLength").asInt())
                    return std::optional<std::string>("string shorter than minLength");
            }
            if (schema_node.has("maxLength") && schema_node.at("maxLength").type() == Dictionary::Integer) {
                if ((int)data.asString().size() > schema_node.at("maxLength").asInt())
                    return std::optional<std::string>("string longer than maxLength");
            }
            // pattern
            if (schema_node.has("pattern") && schema_node.at("pattern").type() == Dictionary::String) {
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
        } else if (t == "integer") {
            if (data.type() != Dictionary::Integer)
                return std::optional<std::string>(
                    limit_line_length("expected type 'integer' at '" + display_path(path) + "' but found '" +
                                      value_type_name(data) + "' (value: " + value_preview(data) + ")"));
            if (auto e = check_numeric_constraints(data, schema_node, path)) return e;
            return std::nullopt;
        } else if (t == "number") {
            if (!(data.type() == Dictionary::Double || data.type() == Dictionary::Integer))
                return std::optional<std::string>(limit_line_length("expected type 'number' at '" + display_path(path) +
                                                                    "' but found '" + value_type_name(data) +
                                                                    "' (value: " + value_preview(data) + ")"));
            if (auto e = check_numeric_constraints(data, schema_node, path)) return e;
            return std::nullopt;
        } else if (t == "boolean") {
            if (data.type() != Dictionary::Boolean)
                return std::optional<std::string>(
                    limit_line_length("expected type 'boolean' at '" + display_path(path) + "' but found '" +
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

}  // namespace ps
