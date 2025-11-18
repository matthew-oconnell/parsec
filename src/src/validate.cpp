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
        auto it = cur->m_object_map.find(token);
        if (it == cur->m_object_map.end()) return nullptr;
        const Dictionary& v = it->second;
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
    auto itmin = schema_node.m_object_map.find("minimum");
    if (itmin != schema_node.m_object_map.end()) {
        const Dictionary& minv = itmin->second;
        if (minv.type() == Dictionary::Integer || minv.type() == Dictionary::Double) {
            double m = (minv.type() == Dictionary::Integer) ? static_cast<double>(minv.asInt()) : minv.asDouble();
            if (val < m)
                return std::optional<std::string>("property '" + path + "' value " + std::to_string(val) +
                                                  " below minimum " + std::to_string(m));
        }
    }
    auto itexmin = schema_node.m_object_map.find("exclusiveMinimum");
    if (itexmin != schema_node.m_object_map.end()) {
        const Dictionary& minv = itexmin->second;
        if (minv.type() == Dictionary::Integer || minv.type() == Dictionary::Double) {
            double m = (minv.type() == Dictionary::Integer) ? static_cast<double>(minv.asInt()) : minv.asDouble();
            if (val <= m)
                return std::optional<std::string>("property '" + path + "' value " + std::to_string(val) +
                                                  " <= exclusiveMinimum " + std::to_string(m));
        }
    }
    auto itmax = schema_node.m_object_map.find("maximum");
    if (itmax != schema_node.m_object_map.end()) {
        const Dictionary& maxv = itmax->second;
        if (maxv.type() == Dictionary::Integer || maxv.type() == Dictionary::Double) {
            double M = (maxv.type() == Dictionary::Integer) ? static_cast<double>(maxv.asInt()) : maxv.asDouble();
            if (val > M)
                return std::optional<std::string>("property '" + path + "' value " + std::to_string(val) +
                                                  " above maximum " + std::to_string(M));
        }
    }
    auto itexmax = schema_node.m_object_map.find("exclusiveMaximum");
    if (itexmax != schema_node.m_object_map.end()) {
        const Dictionary& maxv = itexmax->second;
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
    auto it = schema_node.m_object_map.find("enum");
    if (it == schema_node.m_object_map.end()) return std::nullopt;
    const Dictionary& ev = it->second;
    if (!ev.isArrayObject()) return std::nullopt;
    for (auto const& candidate : ev.m_object_array) {
        if (candidate == data) return std::nullopt;
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
    auto itref = schema_node.m_object_map.find("$ref");
    if (itref != schema_node.m_object_map.end() && itref->second.type() == Dictionary::String) {
        const std::string ref = itref->second.asString();
        const Dictionary* target = resolve_local_ref(schema_root, ref);
        if (!target) return std::optional<std::string>("unresolved $ref '" + ref + "' at " + path);
        return validate_node(data, schema_root, *target, path);
    }

    // enum check
    if (schema_node.m_object_map.find("enum") != schema_node.m_object_map.end()) {
        if (auto e = check_enum(data, schema_node, path)) return e;
    }

    // const keyword: value must equal the provided literal
    auto it_const = schema_node.m_object_map.find("const");
    if (it_const != schema_node.m_object_map.end()) {
        if (!(it_const->second == data)) {
            return std::optional<std::string>("property '" + path + "' does not match const value");
        }
    }

    // allOf/anyOf/oneOf - basic handling
    auto it_allOf = schema_node.m_object_map.find("allOf");
    if (it_allOf != schema_node.m_object_map.end() && it_allOf->second.isArrayObject()) {
        for (auto const& sub : it_allOf->second.m_object_array) {
            const Dictionary* subSchema = schema_from_value(schema_root, sub);
            if (!subSchema) continue;
            if (auto err = validate_node(data, schema_root, *subSchema, path)) return err;
        }
    }
    // anyOf
    auto it_anyOf = schema_node.m_object_map.find("anyOf");
    if (it_anyOf != schema_node.m_object_map.end() && it_anyOf->second.isArrayObject()) {
        bool matched = false;
        for (auto const& sub : it_anyOf->second.m_object_array) {
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
    auto it_oneOf = schema_node.m_object_map.find("oneOf");
    if (it_oneOf != schema_node.m_object_map.end() && it_oneOf->second.isArrayObject()) {
        int matches = 0;
        for (auto const& sub : it_oneOf->second.m_object_array) {
            const Dictionary* subSchema = schema_from_value(schema_root, sub);
            if (!subSchema) continue;
            if (!validate_node(data, schema_root, *subSchema, path).has_value()) {
                ++matches;
            }
        }
        if (matches != 1) return std::optional<std::string>("oneOf did not match exactly one schema");
    }

    // type check
    auto it_type = schema_node.m_object_map.find("type");
    if (it_type != schema_node.m_object_map.end() && it_type->second.type() == Dictionary::String) {
        std::string t = it_type->second.asString();
        if (t == "object") {
            // Validate mapped object: properties, required, additionalProperties,
            // patternProperties, minProperties/maxProperties
            if (!data.isMappedObject())
                return std::optional<std::string>(limit_line_length("expected type 'object' at '" + display_path(path) +
                                                                    "' but found '" + value_type_name(data) +
                                                                    "' (value: " + value_preview(data) + ")"));

            // gather property schemas
            const Dictionary* properties = nullptr;
            auto it_props = schema_node.m_object_map.find("properties");
            if (it_props != schema_node.m_object_map.end() && it_props->second.isMappedObject()) {
                properties = &it_props->second;
            }

            // patternProperties
            const Dictionary* patternProps = nullptr;
            auto it_pat = schema_node.m_object_map.find("patternProperties");
            if (it_pat != schema_node.m_object_map.end() && it_pat->second.isMappedObject()) {
                patternProps = &it_pat->second;
            }

            // additionalProperties may be boolean or schema
            auto it_add = schema_node.m_object_map.find("additionalProperties");

            // required can be specified in two styles: per-property boolean or parent-level array
            std::set<std::string> required_names;
            auto it_req_parent = schema_node.m_object_map.find("required");
            if (it_req_parent != schema_node.m_object_map.end()) {
                const Dictionary& r = it_req_parent->second;
                // support string array or object array containing strings
                try {
                    auto svec = r.asStrings();
                    for (auto const& s : svec) required_names.insert(s);
                } catch (...) {
                    if (r.isArrayObject()) {
                        for (auto const& e : r.m_object_array) {
                            if (e.type() == Dictionary::String) required_names.insert(e.asString());
                        }
                    }
                }
            }

            // Enforce parent-level required names even if no explicit "properties" are listed
            for (auto const& rn : required_names) {
                if (data.m_object_map.find(rn) == data.m_object_map.end()) {
                    return std::optional<std::string>("missing required property '" + rn + "'");
                }
            }

            // minProperties/maxProperties
            auto itminp = schema_node.m_object_map.find("minProperties");
            if (itminp != schema_node.m_object_map.end()) {
                if (itminp->second.type() == Dictionary::Integer) {
                    if (static_cast<int>(data.m_object_map.size()) < itminp->second.asInt())
                        return std::optional<std::string>("object has fewer properties than minProperties");
                }
            }
            auto itmaxp = schema_node.m_object_map.find("maxProperties");
            if (itmaxp != schema_node.m_object_map.end()) {
                if (itmaxp->second.type() == Dictionary::Integer) {
                    if (static_cast<int>(data.m_object_map.size()) > itmaxp->second.asInt())
                        return std::optional<std::string>("object has more properties than maxProperties");
                }
            }

            // iterate expected properties
            if (properties) {
                for (auto const& p : properties->m_object_map) {
                    const std::string& key = p.first;
                    const Dictionary& propSchema = p.second;
                    auto itdata = data.m_object_map.find(key);
                    bool has = itdata != data.m_object_map.end();
                    // per-property required as boolean
                    auto itreq = propSchema.m_object_map.find("required");
                    if (!has) {
                        if (itreq != propSchema.m_object_map.end() && itreq->second.type() == Dictionary::Boolean &&
                            itreq->second.asBool())
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
                                itdata->second, schema_root, *subSchema, path.empty() ? key : path + "." + key))
                            return err;
                    }
                }
            }

            // now check each data property for patternProperties/additionalProperties
            for (auto const& dprop : data.m_object_map) {
                const std::string& key = dprop.first;
                bool handled = false;
                if (properties && properties->m_object_map.find(key) != properties->m_object_map.end()) {
                    handled = true;
                }
                // check patternProperties
                if (!handled && patternProps) {
                    for (auto const& pp : patternProps->m_object_map) {
                        try {
                            std::regex rx(pp.first);
                            if (std::regex_match(key, rx)) {
                                const Dictionary* sub = schema_from_value(schema_root, pp.second);
                                if (sub) {
                                    if (auto err = validate_node(
                                            dprop.second, schema_root, *sub, path.empty() ? key : path + "." + key))
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
                if (it_add == schema_node.m_object_map.end()) {
                    // allowed by default
                    continue;
                }
                const Dictionary& ap = it_add->second;
                if (ap.type() == Dictionary::Boolean) {
                    if (!ap.asBool()) {
                        return std::optional<std::string>("property '" + key + "' not allowed");
                    }
                } else {
                    const Dictionary* sub = schema_from_value(schema_root, ap);
                    if (sub) {
                        if (auto err =
                                validate_node(dprop.second, schema_root, *sub, path.empty() ? key : path + "." + key))
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

            const auto& L = data.m_object_array;
            // minItems / maxItems
            auto itmin = schema_node.m_object_map.find("minItems");
            if (itmin != schema_node.m_object_map.end() && itmin->second.type() == Dictionary::Integer) {
                if ((int)L.size() < itmin->second.asInt()) return std::optional<std::string>("array too few items");
            }
            auto itmax = schema_node.m_object_map.find("maxItems");
            if (itmax != schema_node.m_object_map.end() && itmax->second.type() == Dictionary::Integer) {
                if ((int)L.size() > itmax->second.asInt()) return std::optional<std::string>("array too many items");
            }

            // uniqueItems
            auto ituniq = schema_node.m_object_map.find("uniqueItems");
            if (ituniq != schema_node.m_object_map.end() && ituniq->second.type() == Dictionary::Boolean &&
                ituniq->second.asBool()) {
                std::set<std::string> seen;
                for (auto const& el : L) {
                    std::string key = el.dump();
                    if (seen.find(key) != seen.end()) return std::optional<std::string>("array has duplicate items");
                    seen.insert(key);
                }
            }

            // items: schema or tuple
            auto ititems = schema_node.m_object_map.find("items");
            if (ititems != schema_node.m_object_map.end()) {
                const Dictionary& itemsVal = ititems->second;
                if (itemsVal.isArrayObject()) {
                    // tuple validation
                    size_t nSchemas = itemsVal.m_object_array.size();
                    auto itadd = schema_node.m_object_map.find("additionalItems");
                    for (size_t i = 0; i < L.size(); ++i) {
                        if (i < nSchemas) {
                            const Dictionary* sub = schema_from_value(schema_root, itemsVal.m_object_array[i]);
                            if (sub) {
                                if (auto err =
                                        validate_node(L[i], schema_root, *sub, path + "[" + std::to_string(i) + "]"))
                                    return err;
                            }
                        } else {
                            // additionalItems handling
                            if (itadd != schema_node.m_object_map.end()) {
                                if (itadd->second.type() == Dictionary::Boolean) {
                                    if (!itadd->second.asBool())
                                        return std::optional<std::string>("additional tuple items not allowed");
                                } else {
                                    const Dictionary* sub = schema_from_value(schema_root, itadd->second);
                                    if (sub) {
                                        if (auto err = validate_node(
                                                L[i], schema_root, *sub, path + "[" + std::to_string(i) + "]"))
                                            return err;
                                    }
                                }
                            }
                        }
                    }
                } else {
                    const Dictionary* sub = schema_from_value(schema_root, itemsVal);
                    if (sub) {
                        for (size_t i = 0; i < L.size(); ++i) {
                            if (auto err = validate_node(L[i], schema_root, *sub, path + "[" + std::to_string(i) + "]"))
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
            auto itmin = schema_node.m_object_map.find("minLength");
            if (itmin != schema_node.m_object_map.end() && itmin->second.type() == Dictionary::Integer) {
                if ((int)data.asString().size() < itmin->second.asInt())
                    return std::optional<std::string>("string shorter than minLength");
            }
            auto itmax = schema_node.m_object_map.find("maxLength");
            if (itmax != schema_node.m_object_map.end() && itmax->second.type() == Dictionary::Integer) {
                if ((int)data.asString().size() > itmax->second.asInt())
                    return std::optional<std::string>("string longer than maxLength");
            }
            // pattern
            auto itpat = schema_node.m_object_map.find("pattern");
            if (itpat != schema_node.m_object_map.end() && itpat->second.type() == Dictionary::String) {
                try {
                    std::regex rx(itpat->second.asString());
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
        for (auto const& p : schema.m_object_map) {
            if (schema_keys.find(p.first) != schema_keys.end()) {
                contains_schema_keyword = true;
                break;
            }
        }
    }
    if (schema.isMappedObject() && !contains_schema_keyword &&
        schema.m_object_map.find("properties") == schema.m_object_map.end()) {
        Dictionary wrapper;
        wrapper["type"] = std::string("object");
        wrapper["properties"] = schema;
        return validate_node(data, schema, wrapper, "");
    }
    return validate_node(data, schema, schema, "");
}

}  // namespace ps
