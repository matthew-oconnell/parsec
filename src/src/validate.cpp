#include "ps/validate.hpp"
#include <sstream>
#include <vector>
#include <algorithm>

namespace ps {

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

// Forward declaration of recursive validator
static std::optional<std::string> validate_node(const Value& data, const Dictionary& schema_root, const Dictionary& schema_node, const std::string& path);

// Helper: get a child schema dictionary from a Value that is expected to be an object
static const Dictionary* schema_from_value(const Dictionary& root, const Value& v) {
    if (v.isDict()) return v.asDict().get();
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

    // type check
    auto it_type = schema_node.data.find("type");
    if (it_type != schema_node.data.end() && it_type->second.isString()) {
        std::string t = it_type->second.asString();
        if (t == "object") {
            if (!data.isDict()) return std::optional<std::string>("property '" + path + "' expected type object");
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
                    if (obj.has(propName)) {
                        const Value &childVal = obj.data.at(propName);
                        if (propSchema) {
                            if (auto err = validate_node(childVal, schema_root, *propSchema, childPath)) return err;
                        }
                    }
                }
            }

            // additionalProperties: default true
            bool allow_additional = true;
            auto it_add = schema_node.data.find("additionalProperties");
            if (it_add != schema_node.data.end()) {
                if (it_add->second.isBool()) allow_additional = it_add->second.asBool();
                else if (it_add->second.isDict()) allow_additional = true; // schema for additional props not supported in Phase 1
            }
            if (!allow_additional && props) {
                // ensure no keys outside props
                for (auto const &pd : obj.items()) {
                    const std::string &k = pd.first;
                    if (props->data.find(k) == props->data.end()) return std::optional<std::string>("property '" + (path.empty()?k:(path + "." + k)) + " not allowed by additionalProperties=false");
                }
            }

            return std::nullopt;
        }
        else if (t == "array") {
            if (!data.isList()) return std::optional<std::string>("property '" + path + "' expected type array");
            const auto &arr = data.asList();
            // items (single-schema form)
            auto it_items = schema_node.data.find("items");
            if (it_items != schema_node.data.end()) {
                const Value &itemsVal = it_items->second;
                const Dictionary* itemSchema = schema_from_value(schema_root, itemsVal);
                if (itemSchema) {
                    for (size_t i = 0; i < arr.size(); ++i) {
                        std::string childPath = path + "[" + std::to_string(i) + "]";
                        if (auto err = validate_node(arr[i], schema_root, *itemSchema, childPath)) return err;
                    }
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
            if (!data.isString()) return std::optional<std::string>("property '" + path + "' expected type string");
            // minLength / maxLength (optional)
            auto it_minl = schema_node.data.find("minLength");
            if (it_minl != schema_node.data.end() && it_minl->second.isInt()) {
                if ((int)data.asString().size() < it_minl->second.asInt()) return std::optional<std::string>("property '" + path + "' shorter than minLength");
            }
            auto it_maxl = schema_node.data.find("maxLength");
            if (it_maxl != schema_node.data.end() && it_maxl->second.isInt()) {
                if ((int)data.asString().size() > it_maxl->second.asInt()) return std::optional<std::string>("property '" + path + "' longer than maxLength");
            }
            return std::nullopt;
        }
        else if (t == "integer" || t == "number") {
            if (!(data.isInt() || (t == "number" && data.isDouble()))) return std::optional<std::string>("property '" + path + "' expected type " + t);
            if (auto e = check_numeric_constraints(data, schema_node, path)) return e;
            return std::nullopt;
        }
        else if (t == "boolean") {
            if (!data.isBool()) return std::optional<std::string>("property '" + path + "' expected type boolean");
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
    Value vdata = Value(data);
    return validate_node(vdata, schema, schema, "");
}

} // namespace ps
