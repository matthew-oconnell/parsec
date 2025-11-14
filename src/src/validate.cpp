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
    return msg.substr(0, maxlen-3) + "...";
}

// Helper: convert internal path (dot/bracket style) to folder-style display path
static std::string display_path(const std::string &path) {
    if (path.empty()) return std::string("root");
    // replace dots with '/'
    std::string s = path;
    for (char &c : s) if (c == '.') c = '/';
    // convert [N] into /N
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ) {
        if (s[i] == '[') {
            size_t j = s.find(']', i);
            if (j != std::string::npos) {
                // append '/' then the index
                out.push_back('/');
                out.append(s.substr(i+1, j - (i+1)));
                i = j + 1;
                continue;
            }
        }
        out.push_back(s[i]);
        ++i;
    }
    return out;
}

// Helper: return a short type name for a Value
static std::string value_type_name(const Value &v) {
    if (v.isDict()) return "object";
    if (v.isList()) return "array";
    if (v.isString()) return "string";
    if (v.isInt()) return "integer";
    if (v.isDouble()) return "number";
    if (v.isBool()) return "boolean";
    return "unknown";
}

// Helper: short preview of a Value for error messages
static std::string value_preview(const Value &v, size_t maxlen = 80) {
    std::string s = v.dump();
    if (s.size() > maxlen) s = s.substr(0, maxlen-3) + "...";
    return s;
}

// Resolve a local JSON Pointer-style $ref (only supports local refs starting with "#/")
static const Dictionary* resolve_local_ref(const Dictionary& root, const std::string& ref) {
    if (ref.empty()) return nullptr;
    if (ref[0] != '#') return nullptr; // only local refs supported for Phase 1
    // strip leading '#/' if present
    std::string path = ref;
    if (path.size() >= 2 && path[1] == '/') path = path.substr(2);
    else if (path.size() == 1) return &root;

    const Dictionary* cur = &root;
    size_t pos = 0;
    while (pos < path.size()) {
        size_t next = path.find('/', pos);
        std::string token = (next == std::string::npos) ? path.substr(pos) : path.substr(pos, next - pos);
        pos = (next == std::string::npos) ? path.size() : next + 1;
        auto it = cur->data.find(token);
        if (it == cur->data.end()) return nullptr;
        const Value &v = it->second;
        if (!v.isDict()) return nullptr;
        cur = v.asDict().get();
    }
    return cur;
}

// Forward declarations of recursive validators
static std::optional<std::string> validate_node(const Value& data, const Dictionary& schema_root, const Dictionary& schema_node, const std::string& path);
static std::optional<std::string> validate_node(const Dictionary& dataObj, const Dictionary& schema_root, const Dictionary& schema_node, const std::string& path);

// Helper: get a child schema dictionary from a Value that is expected to be an
// object. Some schema positions allow either a schema object or a string
// $ref (local JSON pointer). Accept the schema root so we can resolve local
// refs when a string is provided.
static const Dictionary* schema_from_value(const Dictionary& schema_root, const Value& v) {
    if (v.isDict()) return v.asDict().get();
    // Accept a string as a local $ref (e.g. "#/definitions/foo")
    if (v.isString()) return resolve_local_ref(schema_root, v.asString());
    return nullptr;
}

// Validate a primitive numeric value against minimum/maximum/exclusive bounds in schema_node
static std::optional<std::string> check_numeric_constraints(const Value& data, const Dictionary& schema_node, const std::string& path) {
    if (!(data.isInt() || data.isDouble())) return std::nullopt;
    double val = data.isInt() ? static_cast<double>(data.asInt()) : data.asDouble();
    // minimum
    if (schema_node.data.find("minimum") != schema_node.data.end()) {
        const Value &minv = schema_node.data.at("minimum");
        if (minv.isInt() || minv.isDouble()) {
            double m = minv.isInt() ? static_cast<double>(minv.asInt()) : minv.asDouble();
            if (val < m) return std::optional<std::string>("property '" + path + "' value " + std::to_string(val) + " below minimum " + std::to_string(m));
        }
    }
    if (schema_node.data.find("exclusiveMinimum") != schema_node.data.end()) {
        const Value &minv = schema_node.data.at("exclusiveMinimum");
        if (minv.isInt() || minv.isDouble()) {
            double m = minv.isInt() ? static_cast<double>(minv.asInt()) : minv.asDouble();
            if (val <= m) return std::optional<std::string>("property '" + path + "' value " + std::to_string(val) + " <= exclusiveMinimum " + std::to_string(m));
        }
    }
    if (schema_node.data.find("maximum") != schema_node.data.end()) {
        const Value &maxv = schema_node.data.at("maximum");
        if (maxv.isInt() || maxv.isDouble()) {
            double M = maxv.isInt() ? static_cast<double>(maxv.asInt()) : maxv.asDouble();
            if (val > M) return std::optional<std::string>("property '" + path + "' value " + std::to_string(val) + " above maximum " + std::to_string(M));
        }
    }
    if (schema_node.data.find("exclusiveMaximum") != schema_node.data.end()) {
        const Value &maxv = schema_node.data.at("exclusiveMaximum");
        if (maxv.isInt() || maxv.isDouble()) {
            double M = maxv.isInt() ? static_cast<double>(maxv.asInt()) : maxv.asDouble();
            if (val >= M) return std::optional<std::string>("property '" + path + "' value " + std::to_string(val) + " >= exclusiveMaximum " + std::to_string(M));
        }
    }
    return std::nullopt;
}

// Check enum keyword: schema_node.data["enum"] should be an array of literal values
static std::optional<std::string> check_enum(const Value& data, const Dictionary& schema_node, const std::string& path) {
    auto it = schema_node.data.find("enum");
    if (it == schema_node.data.end()) return std::nullopt;
    const Value &ev = it->second;
    if (!ev.isList()) return std::nullopt;
    for (auto const &candidate : ev.asList()) {
        if (candidate == data) return std::nullopt; // matches one of enum values
    }
    return std::optional<std::string>("property '" + path + "' not one of enum values");
}

static std::optional<std::string> validate_node(const Value& data, const Dictionary& schema_root, const Dictionary& schema_node, const std::string& path) {
    if (std::getenv("PS_VALIDATE_DEBUG")) {
        std::cerr << "validate_node enter: path='" << path << "' data=" << data.dump() << " schema_keys={";
        bool firstk=true; for (auto const &p: schema_node.items()) { if (!firstk) std::cerr << ","; firstk=false; std::cerr << p.first; } std::cerr << "}\n";
    }
    // resolve $ref if present
    auto itref = schema_node.data.find("$ref");
    if (itref != schema_node.data.end() && itref->second.isString()) {
        const std::string ref = itref->second.asString();
        const Dictionary* target = resolve_local_ref(schema_root, ref);
        if (!target) return std::optional<std::string>("unresolved $ref '" + ref + "' at " + path);
        return validate_node(data, schema_root, *target, path);
    }

    // Backwards-compat: some callers (and our tests) pass a "flat" schema where the
    // top-level Dictionary maps property names -> property-spec (instead of using
    // the JSON Schema `properties` keyword). Support this form when we're at the
    // root (empty path) and the schema_node does not declare an explicit `type`.
    if (path.empty() && schema_node.data.find("type") == schema_node.data.end() && !schema_node.data.empty()) {
        // heuristic: if most entries are object specs, treat schema_node as properties
        bool looks_like_props = true;
        for (auto const &p : schema_node.items()) {
            if (!p.second.isDict()) { looks_like_props = false; break; }
        }
        if (looks_like_props) {
            // data must be an object-like Dictionary
            if (!data.isDict()) return std::optional<std::string>("expected object at top-level for schema");
            const Dictionary &obj = *data.asDict();
            for (auto const &p : schema_node.items()) {
                const std::string &propName = p.first;
                const Value &propSpecVal = p.second;
                if (!propSpecVal.isDict()) continue;
                const Dictionary &propSpec = *propSpecVal.asDict();

                bool required = false;
                auto itreq = propSpec.data.find("required");
                if (itreq != propSpec.data.end() && itreq->second.isBool()) required = itreq->second.asBool();

                if (!obj.has(propName)) {
                    if (required) return std::optional<std::string>("missing required property '" + propName + "'");
                    continue;
                }
                const Value &childVal = obj.data.at(propName);
                if (auto err = validate_node(childVal, schema_root, propSpec, propName)) return err;
            }
            return std::nullopt;
        }
    }

    // enum check (applies to any JSON instance)
    if (schema_node.data.find("enum") != schema_node.data.end()) {
        if (auto e = check_enum(data, schema_node, path)) return e;
    }

    // Composition keywords (Phase 2): allOf, anyOf, oneOf
    auto it_allOf = schema_node.data.find("allOf");
    if (it_allOf != schema_node.data.end() && it_allOf->second.isList()) {
        for (auto const &sub : it_allOf->second.asList()) {
            const Dictionary* subSchema = schema_from_value(schema_root, sub);
            if (!subSchema) continue;
            if (auto err = validate_node(data, schema_root, *subSchema, path)) {
                if (std::getenv("PS_VALIDATE_DEBUG")) {
                    std::cerr << "allOf subschema failed: data=" << data.dump() << " subschema=" << (subSchema?subSchema->dump():std::string("<null>")) << " err=" << *err << "\n";
                }
                return err;
            }
        }
        // all passed
    }
    auto it_anyOf = schema_node.data.find("anyOf");
    if (it_anyOf != schema_node.data.end() && it_anyOf->second.isList()) {
        bool ok = false;
        std::vector<std::string> errs;
        for (auto const &sub : it_anyOf->second.asList()) {
            const Dictionary* subSchema = schema_from_value(schema_root, sub);
            if (!subSchema) continue;
            auto err = validate_node(data, schema_root, *subSchema, path);
            if (!err.has_value()) { ok = true; break; }
            errs.push_back(*err);
        }
        if (!ok) return std::optional<std::string>("anyOf: no alternative matched for '" + path + "'");
    }
    auto it_oneOf = schema_node.data.find("oneOf");
    if (it_oneOf != schema_node.data.end() && it_oneOf->second.isList()) {
        int matched = 0;
        std::vector<std::string> lastErrs;
        for (auto const &sub : it_oneOf->second.asList()) {
            const Dictionary* subSchema = schema_from_value(schema_root, sub);
            if (!subSchema) continue;
            auto err = validate_node(data, schema_root, *subSchema, path);
            if (!err.has_value()) matched++;
            else lastErrs.push_back(*err);
        }
        if (matched != 1) return std::optional<std::string>("oneOf: expected exactly one matching alternative for '" + path + "', got " + std::to_string(matched));
    }

    // type check
    auto it_type = schema_node.data.find("type");
    if (it_type != schema_node.data.end() && it_type->second.isString()) {
        std::string t = it_type->second.asString();
        if (t == "object") {
            if (!data.isDict()) return std::optional<std::string>(limit_line_length("expected type 'object' at '" + display_path(path) + "' but found '" + value_type_name(data) + "' (value: " + value_preview(data) + ")"));
            const Dictionary &obj = *data.asDict();

            // required array
            auto it_req = schema_node.data.find("required");
            if (it_req != schema_node.data.end() && it_req->second.isList()) {
                for (auto const &rnameVal : it_req->second.asList()) {
                    if (!rnameVal.isString()) continue;
                    const std::string rname = rnameVal.asString();
                    if (!obj.has(rname)) return std::optional<std::string>("missing required property '" + (path.empty()?rname:(path + "." + rname)) + "'");
                }
            }

            // properties
            const Dictionary* props = nullptr;
            auto it_props = schema_node.data.find("properties");
            if (it_props != schema_node.data.end()) props = schema_from_value(schema_root, it_props->second);

            if (props) {
                for (auto const &p : props->items()) {
                    const std::string &propName = p.first;
                    const Value &propSchemaVal = p.second;
                    const Dictionary* propSchema = schema_from_value(schema_root, propSchemaVal);
                    std::string childPath = path.empty() ? propName : (path + "." + propName);
                    auto itObj = obj.data.find(propName);
                    if (itObj != obj.data.end()) {
                        const Value &childVal = itObj->second;
                        if (propSchema) {
                            if (auto err = validate_node(childVal, schema_root, *propSchema, childPath)) return err;
                        }
                    }
                }
            }

            // patternProperties (regex keyed property schemas)
            auto it_pattern = schema_node.data.find("patternProperties");
            if (it_pattern != schema_node.data.end() && it_pattern->second.isDict()) {
                const Dictionary &pp = *it_pattern->second.asDict();
                for (auto const &ppent : pp.items()) {
                    std::string pattern = ppent.first;
                    const Dictionary* patSchema = schema_from_value(schema_root, ppent.second);
                    std::regex re;
                    try { re = std::regex(pattern); } catch (...) { continue; }
                    for (auto const &od : obj.items()) {
                        if (std::regex_match(od.first, re)) {
                            if (patSchema) {
                                std::string childPath = path.empty() ? od.first : (path + "." + od.first);
                                if (auto err = validate_node(od.second, schema_root, *patSchema, childPath)) return err;
                            }
                        }
                    }
                }
            }

            // additionalProperties: default true
            bool allow_additional = true;
            auto it_add = schema_node.data.find("additionalProperties");
            const Dictionary* addPropsSchema = nullptr;
            if (it_add != schema_node.data.end()) {
                if (it_add->second.isBool()) allow_additional = it_add->second.asBool();
                else if (it_add->second.isDict()) {
                    allow_additional = true; // additional properties allowed but must validate against schema
                    addPropsSchema = it_add->second.asDict().get();
                }
            }
            if (!allow_additional && props) {
                // ensure no keys outside props
                for (auto const &pd : obj.items()) {
                    const std::string &k = pd.first;
                    if (props->data.find(k) == props->data.end()) return std::optional<std::string>("property '" + (path.empty()?k:(path + "." + k)) + " not allowed by additionalProperties=false");
                }
            }
            // If additionalProperties is a schema, validate any keys not in properties or patternProperties
            if (addPropsSchema) {
                // Build a set of property names covered by 'properties'
                std::set<std::string> covered;
                if (props) {
                    for (auto const &p : props->items()) covered.insert(p.first);
                }
                // patternProperties keys
                std::vector<std::pair<std::regex,const Dictionary*>> pattern_schemas;
                auto it_pattern = schema_node.data.find("patternProperties");
                if (it_pattern != schema_node.data.end() && it_pattern->second.isDict()) {
                    const Dictionary &pp = *it_pattern->second.asDict();
                    for (auto const &ppent : pp.items()) {
                        try { pattern_schemas.emplace_back(std::regex(ppent.first), ppent.second.isDict()?ppent.second.asDict().get():nullptr); }
                        catch(...) { }
                    }
                }
                for (auto const &od : obj.items()) {
                    const std::string &k = od.first;
                    if (covered.find(k) != covered.end()) continue;
                    bool matched_pattern = false;
                    for (auto const &ps : pattern_schemas) {
                        if (std::regex_match(k, ps.first)) { matched_pattern = true; break; }
                    }
                    if (matched_pattern) continue;
                    // validate this additional property against addPropsSchema
                    if (auto err = validate_node(od.second, schema_root, *addPropsSchema, path.empty()?k:(path + "." + k))) return err;
                }
            }

            // minProperties / maxProperties (object property count constraints)
            auto it_minp = schema_node.data.find("minProperties");
            if (it_minp != schema_node.data.end() && it_minp->second.isInt()) {
                int minp = it_minp->second.asInt();
                if (static_cast<int>(obj.size()) < minp) return std::optional<std::string>("object '" + path + "' has fewer properties than minProperties");
            }
            auto it_maxp = schema_node.data.find("maxProperties");
            if (it_maxp != schema_node.data.end() && it_maxp->second.isInt()) {
                int maxp = it_maxp->second.asInt();
                if (static_cast<int>(obj.size()) > maxp) return std::optional<std::string>("object '" + path + "' has more properties than maxProperties");
            }

            return std::nullopt;
        }
        else if (t == "array") {
            if (!data.isList()) return std::optional<std::string>(limit_line_length("expected type 'array' at '" + display_path(path) + "' but found '" + value_type_name(data) + "' (value: " + value_preview(data) + ")"));
            const auto &arr = data.asList();
            // items can be either a single schema or an array of schemas (tuple validation)
            auto it_items = schema_node.data.find("items");
            if (it_items != schema_node.data.end()) {
                const Value &itemsVal = it_items->second;
                if (itemsVal.isList()) {
                    // tuple-style
                    const auto &schemas = itemsVal.asList();
                    for (size_t i = 0; i < arr.size() && i < schemas.size(); ++i) {
                        const Dictionary* itemSchema = schema_from_value(schema_root, schemas[i]);
                        if (itemSchema) {
                            std::string childPath = path + "[" + std::to_string(i) + "]";
                            if (auto err = validate_node(arr[i], schema_root, *itemSchema, childPath)) return err;
                        }
                    }
                    // handle additionalItems
                    if (arr.size() > schemas.size()) {
                        auto it_add = schema_node.data.find("additionalItems");
                        if (it_add != schema_node.data.end()) {
                            if (it_add->second.isBool()) {
                                if (!it_add->second.asBool()) return std::optional<std::string>("array '" + path + "' has additional items but additionalItems=false");
                            } else if (it_add->second.isDict()) {
                                const Dictionary* addSchema = schema_from_value(schema_root, it_add->second);
                                if (addSchema) {
                                    for (size_t i = schemas.size(); i < arr.size(); ++i) {
                                        std::string childPath = path + "[" + std::to_string(i) + "]";
                                        if (auto err = validate_node(arr[i], schema_root, *addSchema, childPath)) return err;
                                    }
                                }
                            }
                        }
                    }
                } else {
                    // single-schema form
                    const Dictionary* itemSchema = schema_from_value(schema_root, itemsVal);
                    if (itemSchema) {
                        for (size_t i = 0; i < arr.size(); ++i) {
                            std::string childPath = path + "[" + std::to_string(i) + "]";
                            if (auto err = validate_node(arr[i], schema_root, *itemSchema, childPath)) return err;
                        }
                    }
                }
            }

            // uniqueItems
            auto it_unique = schema_node.data.find("uniqueItems");
            if (it_unique != schema_node.data.end() && it_unique->second.isBool() && it_unique->second.asBool()) {
                std::set<std::string> seen;
                for (auto const &el : arr) {
                    std::string s = el.dump();
                    if (seen.find(s) != seen.end()) return std::optional<std::string>("array '" + path + "' contains non-unique items");
                    seen.insert(s);
                }
            }

            // minItems / maxItems
            auto it_min = schema_node.data.find("minItems");
            if (it_min != schema_node.data.end() && it_min->second.isInt()) {
                if (static_cast<int>(arr.size()) < it_min->second.asInt()) return std::optional<std::string>("array '" + path + "' has fewer items than minItems");
            }
            auto it_max = schema_node.data.find("maxItems");
            if (it_max != schema_node.data.end() && it_max->second.isInt()) {
                if (static_cast<int>(arr.size()) > it_max->second.asInt()) return std::optional<std::string>("array '" + path + "' has more items than maxItems");
            }
            return std::nullopt;
        }
        else if (t == "string") {
            if (!data.isString()) return std::optional<std::string>(limit_line_length("expected type 'string' at '" + display_path(path) + "' but found '" + value_type_name(data) + "' (value: " + value_preview(data) + ")"));
            // minLength / maxLength (optional)
            auto it_minl = schema_node.data.find("minLength");
            if (it_minl != schema_node.data.end() && it_minl->second.isInt()) {
                if ((int)data.asString().size() < it_minl->second.asInt()) return std::optional<std::string>("property '" + path + "' shorter than minLength");
            }
            auto it_maxl = schema_node.data.find("maxLength");
            if (it_maxl != schema_node.data.end() && it_maxl->second.isInt()) {
                if ((int)data.asString().size() > it_maxl->second.asInt()) return std::optional<std::string>("property '" + path + "' longer than maxLength");
            }
            // pattern (regular expression) - if present, the string must match
            auto it_pattern = schema_node.data.find("pattern");
            if (it_pattern != schema_node.data.end() && it_pattern->second.isString()) {
                const std::string pat = it_pattern->second.asString();
                try {
                    std::regex re(pat);
                    if (!std::regex_match(data.asString(), re)) return std::optional<std::string>("property '" + path + "' does not match pattern");
                } catch (...) {
                    // invalid regex in schema: ignore pattern constraint
                }
            }
            return std::nullopt;
        }
        else if (t == "integer" || t == "number") {
            if (!(data.isInt() || (t == "number" && data.isDouble()))) return std::optional<std::string>(limit_line_length("expected type '" + t + "' at '" + display_path(path) + "' but found '" + value_type_name(data) + "' (value: " + value_preview(data) + ")"));
            if (auto e = check_numeric_constraints(data, schema_node, path)) return e;
            return std::nullopt;
        }
        else if (t == "boolean") {
            if (!data.isBool()) return std::optional<std::string>(limit_line_length("expected type 'boolean' at '" + display_path(path) + "' but found '" + value_type_name(data) + "' (value: " + value_preview(data) + ")"));
            return std::nullopt;
        }
    }

    // If no type declared, still apply numeric/enum constraints if provided
    if (auto e = check_enum(data, schema_node, path)) return e;
    if (auto e2 = check_numeric_constraints(data, schema_node, path)) return e2;

    return std::nullopt;
}

std::optional<std::string> validate(const Dictionary& data, const Dictionary& schema) {
    // Top-level schema is expected to be a JSON Schema-like object. We validate the
    // provided `data` (a Dictionary) against `schema`. For recursive checks we pass
    // the same `schema` as the root for $ref resolution.
    // If the caller passed a Dictionary that wraps a scalar (or an array) in
    // its `scalar` member, prefer that Value directly so we validate the actual
    // JSON value instead of a wrapper object. This keeps tests and callers that
    // use Dictionary::scalar working as expected.
    Value vdata;
    if (data.scalar) vdata = *data.scalar;
    else vdata = Value(data);
    auto res = validate_node(vdata, schema, schema, "");
    if (std::getenv("PS_VALIDATE_DEBUG")) {
        std::cerr << "validate debug: data=" << vdata.dump() << " schema=" << schema.dump() << " result=" << (res?*res:"<ok>") << "\n";
    }
    return res;
}

// Small wrapper overload: accept a Dictionary directly and forward to the
// Value-based validator. This allows callers to pass a Dictionary without
// duplicating the conversion logic at call sites during incremental
// migration.
static std::optional<std::string> validate_node(const Dictionary& dataObj, const Dictionary& schema_root, const Dictionary& schema_node, const std::string& path) {
    Value vdata;
    if (dataObj.scalar) vdata = *dataObj.scalar;
    else vdata = Value(dataObj);
    return validate_node(vdata, schema_root, schema_node, path);
}

} // namespace ps
