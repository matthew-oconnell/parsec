#include "ps/validate.h"

namespace ps {

// Helper: deep-copy a Dictionary (copy constructor performs deep copy)
static Dictionary clone_value(const Dictionary& v) { return v; }

// Resolve a local JSON Pointer-style $ref (only supports local refs starting with "#/")
static const Dictionary* resolve_local_ref(const Dictionary& root, const std::string& ref) {
    if (ref.empty()) return nullptr;
    if (ref[0] != '#') return nullptr;  // only local refs supported
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

// Recursively apply defaults from schema_node into target (which may be a dict or scalar)
static Dictionary apply_defaults_to_value(const Dictionary& dataVal,
                                          const Dictionary& schema_root,
                                          const Dictionary& schema_node);

static Dictionary apply_defaults_to_object(const Dictionary& input,
                                           const Dictionary& schema_root,
                                           const Dictionary& schema_node) {
    Dictionary out = input;  // copy existing

    // properties
    const Dictionary* props = nullptr;
    if (schema_node.has("properties") && schema_node.at("properties").isMappedObject())
        props = &schema_node.at("properties");

    if (props) {
        for (auto const& p : props->items()) {
            const std::string& k = p.first;
            const Dictionary& propSchemaVal = p.second;
            
            // Resolve $ref if present
            const Dictionary* propSchema = nullptr;
            if (propSchemaVal.isMappedObject()) {
                if (propSchemaVal.has("$ref") && propSchemaVal.at("$ref").type() == Dictionary::String) {
                    const std::string ref = propSchemaVal.at("$ref").asString();
                    propSchema = resolve_local_ref(schema_root, ref);
                } else {
                    propSchema = &propSchemaVal;
                }
            }
            

            // If key present in input, recursively apply defaults into it
            if (out.has(k)) {
                if (propSchema) {
                    if (out[k].isMappedObject()) {
                        Dictionary nested = out[k];
                        Dictionary nested_with = apply_defaults_to_object(nested, schema_root, *propSchema);
                        out[k] = nested_with;
                    } else {
                        Dictionary existing = out[k];
                        Dictionary withDefaults = apply_defaults_to_value(existing, schema_root, *propSchema);
                        out[k] = withDefaults;
                    }
                }
            } else {
                // Not present: if schema has a default, use it; else if propSchema is an object
                // with nested defaults, we may create an empty object and apply nested defaults if
                // there are defaults deeper in the schema.
                if (propSchema && propSchema->has("default")) {
                    out[k] = clone_value(propSchema->at("default"));
                } else if (propSchema && propSchema->has("type") &&
                           propSchema->at("type").type() == Dictionary::String &&
                           propSchema->at("type").asString() == "object") {
                    // If nested object may have defaults inside, create an empty object and apply
                    // nested defaults
                    Dictionary empty;
                    Dictionary nested = apply_defaults_to_object(empty, schema_root, *propSchema);
                    if (!nested.empty()) out[k] = nested;
                }
            }
        }
    }

    // additionalProperties: if schema object with default, and keys in input not covered by
    // properties, ensure defaults applied
    const Dictionary* addSchema = nullptr;
    if (schema_node.has("additionalProperties") && schema_node.at("additionalProperties").isMappedObject())
        addSchema = &schema_node.at("additionalProperties");

    if (addSchema) {
        for (auto const& pd : out.items()) {
            const std::string& k = pd.first;
            if (props && props->has(k)) continue;  // covered by properties
            // apply defaults into this additional property
            if (pd.second.isMappedObject()) {
                Dictionary nested = pd.second;
                Dictionary nested_with = apply_defaults_to_object(nested, schema_root, *addSchema);
                out[k] = nested_with;
            } else {
                Dictionary existing = pd.second;
                Dictionary withDefaults = apply_defaults_to_value(existing, schema_root, *addSchema);
                out[k] = withDefaults;
            }
        }
    }

    return out;
}

static Dictionary apply_defaults_to_value(const Dictionary& dataVal,
                                          const Dictionary& schema_root,
                                          const Dictionary& schema_node) {
    // Resolve $ref if present
    const Dictionary* actual_schema = &schema_node;
    if (schema_node.has("$ref") && schema_node.at("$ref").type() == Dictionary::String) {
        const std::string ref = schema_node.at("$ref").asString();
        const Dictionary* resolved = resolve_local_ref(schema_root, ref);
        if (resolved) {
            actual_schema = resolved;
        }
    }

    // If schema_node has a 'default' and dataVal is explicitly null, return default.
    // Do NOT treat scalar values (int/string/bool) as "missing" — those are user
    // provided values and must be preserved. Only apply the schema default when
    // the provided value is actually null/absent.
    if (dataVal.type() == Dictionary::Null) {
        if (actual_schema->has("default")) return clone_value(actual_schema->at("default"));
    }

    // If schema type is object, recurse
    if (actual_schema->has("type") && actual_schema->at("type").type() == Dictionary::String &&
        actual_schema->at("type").asString() == "object") {
        Dictionary inObj;
        if (dataVal.isMappedObject()) inObj = dataVal;
        Dictionary outObj = apply_defaults_to_object(inObj, schema_root, *actual_schema);
        return outObj;
    }

    // If type is array
    if (actual_schema->has("type") && actual_schema->at("type").type() == Dictionary::String &&
        actual_schema->at("type").asString() == "array") {
        if (!dataVal.isArrayObject()) {
            if (actual_schema->has("default")) return clone_value(actual_schema->at("default"));
        } else {
            if (actual_schema->has("items") && actual_schema->at("items").isMappedObject() && dataVal.isArrayObject()) {
                const Dictionary* itemSchema = &actual_schema->at("items");
                std::vector<Dictionary> outList;
                for (int i = 0; i < dataVal.size(); ++i) {
                    const Dictionary& el = dataVal[i];
                    outList.push_back(apply_defaults_to_value(el, schema_root, *itemSchema));
                }
                Dictionary arr;
                arr = outList;
                return arr;
            }
        }
    }

    // Otherwise return dataVal unchanged
    return dataVal;
}

Dictionary setDefaults(const Dictionary& data, const Dictionary& schema) {
    // Use schema as root for $ref resolution in future work; for now pass schema through

    // Top-level: if schema declares type object, apply object defaults; otherwise if default
    // exists, use it
    if (schema.has("type") && schema.at("type").type() == Dictionary::String &&
        schema.at("type").asString() == "object") {
        Dictionary inObj;
        if (data.isMappedObject()) inObj = data;
        Dictionary outObj = apply_defaults_to_object(inObj, schema, schema);
        return outObj;
    }

    // If schema has a top-level default and data is not object/array, return default
    if ((!data.isMappedObject() && !data.isArrayObject()) && schema.has("default")) {
        return clone_value(schema.at("default"));
    }

    // Otherwise return the input dictionary unchanged
    return data;
}

}  // namespace ps
