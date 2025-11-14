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
        case Dictionary::String: return "string";
        case Dictionary::Integer: return "integer";
        case Dictionary::Double: return "number";
        case Dictionary::Boolean: return "boolean";
        case Dictionary::Null: return "null";
        default: return "unknown";
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
                return std::optional<std::string>("property '" + path + "' value " +
                                                  std::to_string(val) + " below minimum " +
                                                  std::to_string(m));
        }
    }
    auto itexmin = schema_node.m_object_map.find("exclusiveMinimum");
    if (itexmin != schema_node.m_object_map.end()) {
        const Dictionary& minv = itexmin->second;
        if (minv.type() == Dictionary::Integer || minv.type() == Dictionary::Double) {
            double m = (minv.type() == Dictionary::Integer) ? static_cast<double>(minv.asInt()) : minv.asDouble();
            if (val <= m)
                return std::optional<std::string>("property '" + path + "' value " +
                                                  std::to_string(val) + " <= exclusiveMinimum " +
                                                  std::to_string(m));
        }
    }
    auto itmax = schema_node.m_object_map.find("maximum");
    if (itmax != schema_node.m_object_map.end()) {
        const Dictionary& maxv = itmax->second;
        if (maxv.type() == Dictionary::Integer || maxv.type() == Dictionary::Double) {
            double M = (maxv.type() == Dictionary::Integer) ? static_cast<double>(maxv.asInt()) : maxv.asDouble();
            if (val > M)
                return std::optional<std::string>("property '" + path + "' value " +
                                                  std::to_string(val) + " above maximum " +
                                                  std::to_string(M));
        }
    }
    auto itexmax = schema_node.m_object_map.find("exclusiveMaximum");
    if (itexmax != schema_node.m_object_map.end()) {
        const Dictionary& maxv = itexmax->second;
        if (maxv.type() == Dictionary::Integer || maxv.type() == Dictionary::Double) {
            double M = (maxv.type() == Dictionary::Integer) ? static_cast<double>(maxv.asInt()) : maxv.asDouble();
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

    // allOf/anyOf/oneOf - basic handling
    auto it_allOf = schema_node.m_object_map.find("allOf");
    if (it_allOf != schema_node.m_object_map.end() && it_allOf->second.isArrayObject()) {
        for (auto const& sub : it_allOf->second.m_object_array) {
            const Dictionary* subSchema = schema_from_value(schema_root, sub);
            if (!subSchema) continue;
            if (auto err = validate_node(data, schema_root, *subSchema, path)) return err;
        }
    }

    // type check
    auto it_type = schema_node.m_object_map.find("type");
    if (it_type != schema_node.m_object_map.end() && it_type->second.type() == Dictionary::String) {
        std::string t = it_type->second.asString();
        if (t == "object") {
            if (!data.isMappedObject())
                return std::optional<std::string>(limit_line_length(
                            "expected type 'object' at '" + display_path(path) + "' but found '" +
                            value_type_name(data) + "' (value: " + value_preview(data) + ")"));
            // minimal: don't recurse into properties here for compilation purposes
            return std::nullopt;
        } else if (t == "array") {
            if (!data.isArrayObject())
                return std::optional<std::string>(limit_line_length(
                            "expected type 'array' at '" + display_path(path) + "' but found '" +
                            value_type_name(data) + "' (value: " + value_preview(data) + ")"));
            return std::nullopt;
        } else if (t == "string") {
            if (data.type() != Dictionary::String)
                return std::optional<std::string>(limit_line_length(
                            "expected type 'string' at '" + display_path(path) + "' but found '" +
                            value_type_name(data) + "' (value: " + value_preview(data) + ")"));
            return std::nullopt;
        } else if (t == "integer") {
            if (data.type() != Dictionary::Integer)
                return std::optional<std::string>(limit_line_length(
                            "expected type 'integer' at '" + display_path(path) + "' but found '" +
                            value_type_name(data) + "' (value: " + value_preview(data) + ")"));
            if (auto e = check_numeric_constraints(data, schema_node, path)) return e;
            return std::nullopt;
        } else if (t == "number") {
            if (!(data.type() == Dictionary::Double || data.type() == Dictionary::Integer))
                return std::optional<std::string>(limit_line_length(
                            "expected type 'number' at '" + display_path(path) + "' but found '" +
                            value_type_name(data) + "' (value: " + value_preview(data) + ")"));
            if (auto e = check_numeric_constraints(data, schema_node, path)) return e;
            return std::nullopt;
        } else if (t == "boolean") {
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
    return validate_node(data, schema, schema, "");
}

}  // namespace ps
