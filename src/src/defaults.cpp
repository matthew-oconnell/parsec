#include "ps/validate.h"

namespace ps {

// Helper: deep-copy a Dictionary (copy constructor performs deep copy)
static Dictionary clone_value(const Dictionary& v) { return v; }

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
    auto it_props = schema_node.m_object_map.find("properties");
    if (it_props != schema_node.m_object_map.end()) props = &it_props->second;

    if (props) {
        for (auto const& p : props->items()) {
            const std::string& k = p.first;
            const Dictionary& propSchemaVal = p.second;
            const Dictionary* propSchema =
                        propSchemaVal.isMappedObject() ? &propSchemaVal : nullptr;

            // If key present in input, recursively apply defaults into it
            if (out.has(k)) {
                if (propSchema) {
                    if (out[k].isMappedObject()) {
                        Dictionary nested = out[k];
                        Dictionary nested_with =
                                    apply_defaults_to_object(nested, schema_root, *propSchema);
                        out[k] = nested_with;
                    } else {
                        Dictionary existing = out[k];
                        Dictionary withDefaults =
                                    apply_defaults_to_value(existing, schema_root, *propSchema);
                        out[k] = withDefaults;
                    }
                }
            } else {
                // Not present: if schema has a default, use it; else if propSchema is an object
                // with nested defaults, we may create an empty object and apply nested defaults if
                // there are defaults deeper in the schema.
                if (propSchema &&
                    propSchema->m_object_map.find("default") != propSchema->m_object_map.end()) {
                    out[k] = clone_value(propSchema->m_object_map.at("default"));
                } else if (propSchema &&
                           propSchema->m_object_map.find("type") !=
                                       propSchema->m_object_map.end() &&
                           propSchema->m_object_map.at("type").type() == Dictionary::String &&
                           propSchema->m_object_map.at("type").asString() == "object") {
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
    auto it_add = schema_node.m_object_map.find("additionalProperties");
    const Dictionary* addSchema = nullptr;
    if (it_add != schema_node.m_object_map.end() && it_add->second.isMappedObject())
        addSchema = &it_add->second;

    if (addSchema) {
        for (auto const& pd : out.items()) {
            const std::string& k = pd.first;
            if (props && props->m_object_map.find(k) != props->m_object_map.end())
                continue;  // covered by properties
            // apply defaults into this additional property
            if (pd.second.isMappedObject()) {
                Dictionary nested = pd.second;
                Dictionary nested_with = apply_defaults_to_object(nested, schema_root, *addSchema);
                out[k] = nested_with;
            } else {
                Dictionary existing = pd.second;
                Dictionary withDefaults =
                            apply_defaults_to_value(existing, schema_root, *addSchema);
                out[k] = withDefaults;
            }
        }
    }

    return out;
}

static Dictionary apply_defaults_to_value(const Dictionary& dataVal,
                                          const Dictionary& schema_root,
                                          const Dictionary& schema_node) {
    // If schema_node has a 'default' and dataVal is null/empty, return default
    if ((dataVal.type() == Dictionary::Null ||
         (!dataVal.isMappedObject() && !dataVal.isArrayObject()))) {
        auto it_def = schema_node.m_object_map.find("default");
        if (it_def != schema_node.m_object_map.end()) return clone_value(it_def->second);
    }

    // If schema type is object, recurse
    auto it_type = schema_node.m_object_map.find("type");
    if (it_type != schema_node.m_object_map.end() && it_type->second.type() == Dictionary::String &&
        it_type->second.asString() == "object") {
        Dictionary inObj;
        if (dataVal.isMappedObject()) inObj = dataVal;
        Dictionary outObj = apply_defaults_to_object(inObj, schema_root, schema_node);
        return outObj;
    }

    // If type is array
    if (it_type != schema_node.m_object_map.end() && it_type->second.type() == Dictionary::String &&
        it_type->second.asString() == "array") {
        if (!dataVal.isArrayObject()) {
            auto it_def = schema_node.m_object_map.find("default");
            if (it_def != schema_node.m_object_map.end()) return clone_value(it_def->second);
        } else {
            auto it_items = schema_node.m_object_map.find("items");
            if (it_items != schema_node.m_object_map.end() && it_items->second.isMappedObject() &&
                dataVal.isArrayObject()) {
                const Dictionary* itemSchema = &it_items->second;
                std::vector<Dictionary> outList;
                for (auto const& el : dataVal.m_object_array) {
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
    auto it_type = schema.m_object_map.find("type");
    if (it_type != schema.m_object_map.end() && it_type->second.type() == Dictionary::String &&
        it_type->second.asString() == "object") {
        Dictionary inObj;
        if (data.isMappedObject()) inObj = data;
        Dictionary outObj = apply_defaults_to_object(inObj, schema, schema);
        return outObj;
    }

    // If schema has a top-level default and data is not object/array, return default
    if ((!data.isMappedObject() && !data.isArrayObject()) &&
        schema.m_object_map.find("default") != schema.m_object_map.end()) {
        return clone_value(schema.m_object_map.at("default"));
    }

    // Otherwise return the input dictionary unchanged
    return data;
}

}  // namespace ps
