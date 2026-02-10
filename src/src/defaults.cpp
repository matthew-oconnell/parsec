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

// Recursively apply defaults from schema_node into target (which may be a dict or scalar)
static Dictionary apply_defaults_to_value(const Dictionary& dataVal,
                                          const Dictionary& schema_root,
                                          const Dictionary& schema_node);

static Dictionary apply_defaults_to_object(const Dictionary& input,
                                           const Dictionary& schema_root,
                                           const Dictionary& schema_node) {
    Dictionary out = input;  // copy existing

    // Handle allOf: recursively apply defaults from each schema in the array
    if (schema_node.has("allOf") && schema_node.at("allOf").isArrayObject()) {
        const Dictionary& allOfArray = schema_node.at("allOf");
        for (int i = 0; i < allOfArray.size(); ++i) {
            const Dictionary* subSchema = &allOfArray[i];

            // Resolve $ref if present
            if (subSchema->has("$ref") && subSchema->at("$ref").type() == Dictionary::String) {
                const std::string ref = subSchema->at("$ref").asString();
                subSchema = resolve_local_ref(schema_root, ref);
                if (!subSchema) continue;
            }

            // Recursively apply defaults from this sub-schema
            out = apply_defaults_to_object(out, schema_root, *subSchema);
        }
        return out;
    }

    // properties
    const Dictionary* props = nullptr;
    if (schema_node.has("properties") && schema_node.at("properties").isMappedObject())
        props = &schema_node.at("properties");

    if (props) {
        for (auto const& p : props->items()) {
            const std::string& k = p.first;
            const Dictionary& propSchemaVal = p.second;

            // Resolve $ref if present, but keep track of original for default value
            const Dictionary* propSchema = nullptr;
            const Dictionary* propSchemaForDefault = &propSchemaVal;  // May have 'default' key
            if (propSchemaVal.isMappedObject()) {
                if (propSchemaVal.has("$ref") &&
                    propSchemaVal.at("$ref").type() == Dictionary::String) {
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
                // Not present: check for default in the original schema (before $ref resolution)
                // or in the resolved schema
                if (propSchemaForDefault->has("default")) {
                    out[k] = clone_value(propSchemaForDefault->at("default"));
                } else if (propSchema && propSchema->has("default")) {
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
    if (schema_node.has("additionalProperties") &&
        schema_node.at("additionalProperties").isMappedObject())
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
    // Also treat schemas with "properties" as object schemas even without explicit type
    bool isObjectSchema = false;
    if (actual_schema->has("type") && actual_schema->at("type").type() == Dictionary::String &&
        actual_schema->at("type").asString() == "object") {
        isObjectSchema = true;
    }
    if (actual_schema->has("properties")) {
        isObjectSchema = true;
    }
    
    if (isObjectSchema) {
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
            if (actual_schema->has("items") && actual_schema->at("items").isMappedObject() &&
                dataVal.isArrayObject()) {
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

    // Handle anyOf: try each alternative and apply defaults from the first matching schema
    // We determine a match by checking if the data is an object with a "type" field that
    // matches one of the alternatives, or by trying to apply and checking for consistency.
    if (actual_schema->has("anyOf") && actual_schema->at("anyOf").isArrayObject()) {
        const Dictionary& anyOfArray = actual_schema->at("anyOf");

        // If dataVal is an object with a "type" field, try to find matching schema
        if (dataVal.isMappedObject() && dataVal.has("type")) {
            std::string dataType = dataVal.at("type").asString();

            // Try each alternative to find one with matching type enum
            for (int i = 0; i < anyOfArray.size(); ++i) {
                const Dictionary* altSchema = &anyOfArray[i];

                // Resolve $ref if present
                if (altSchema->has("$ref") && altSchema->at("$ref").type() == Dictionary::String) {
                    const std::string ref = altSchema->at("$ref").asString();
                    altSchema = resolve_local_ref(schema_root, ref);
                    if (!altSchema) continue;
                }

                // Check if this alternative has type enum matching our data
                if (altSchema->has("properties") && altSchema->at("properties").has("type")) {
                    const Dictionary& typeProp = altSchema->at("properties").at("type");
                    if (typeProp.has("enum") && typeProp.at("enum").isArrayObject()) {
                        const Dictionary& enumArray = typeProp.at("enum");
                        for (int j = 0; j < enumArray.size(); ++j) {
                            if (enumArray[j].type() == Dictionary::String &&
                                enumArray[j].asString() == dataType) {
                                // Found matching schema - apply defaults from it
                                return apply_defaults_to_value(dataVal, schema_root, *altSchema);
                            }
                        }
                    }
                }
            }
        }

        // If no type-based match, just try the first alternative
        if (anyOfArray.size() > 0) {
            return apply_defaults_to_value(dataVal, schema_root, anyOfArray[0]);
        }
    }

    // Handle oneOf similarly to anyOf
    if (actual_schema->has("oneOf") && actual_schema->at("oneOf").isArrayObject()) {
        const Dictionary& oneOfArray = actual_schema->at("oneOf");

        if (dataVal.isMappedObject() && dataVal.has("type")) {
            std::string dataType = dataVal.at("type").asString();

            for (int i = 0; i < oneOfArray.size(); ++i) {
                const Dictionary* altSchema = &oneOfArray[i];

                if (altSchema->has("$ref") && altSchema->at("$ref").type() == Dictionary::String) {
                    const std::string ref = altSchema->at("$ref").asString();
                    altSchema = resolve_local_ref(schema_root, ref);
                    if (!altSchema) continue;
                }

                if (altSchema->has("properties") && altSchema->at("properties").has("type")) {
                    const Dictionary& typeProp = altSchema->at("properties").at("type");
                    if (typeProp.has("enum") && typeProp.at("enum").isArrayObject()) {
                        const Dictionary& enumArray = typeProp.at("enum");
                        for (int j = 0; j < enumArray.size(); ++j) {
                            if (enumArray[j].type() == Dictionary::String &&
                                enumArray[j].asString() == dataType) {
                                return apply_defaults_to_value(dataVal, schema_root, *altSchema);
                            }
                        }
                    }
                }
            }
        }

        if (oneOfArray.size() > 0) {
            return apply_defaults_to_value(dataVal, schema_root, oneOfArray[0]);
        }
    }

    // Otherwise return dataVal unchanged
    return dataVal;
}

Dictionary setDefaults(const Dictionary& data, const Dictionary& schema) {
    // Use schema as root for $ref resolution in future work; for now pass schema through

    // Resolve root-level $ref if present
    const Dictionary* effectiveSchema = &schema;
    if (schema.has("$ref") && schema.at("$ref").type() == Dictionary::String) {
        const Dictionary* resolved = resolve_local_ref(schema, schema.at("$ref").asString());
        if (resolved) {
            effectiveSchema = resolved;
        }
    }

    // Top-level: if schema declares type object OR has properties (implying object type),
    // apply object defaults; otherwise if default exists, use it
    bool isObjectSchema = false;
    if (effectiveSchema->has("type") && effectiveSchema->at("type").type() == Dictionary::String &&
        effectiveSchema->at("type").asString() == "object") {
        isObjectSchema = true;
    }
    // Also treat schemas with "properties" as object schemas even without explicit type
    if (effectiveSchema->has("properties")) {
        isObjectSchema = true;
    }
    
    if (isObjectSchema) {
        Dictionary inObj;
        if (data.isMappedObject()) inObj = data;
        Dictionary outObj = apply_defaults_to_object(inObj, schema, *effectiveSchema);
        return outObj;
    }

    // If schema has a top-level default and data is not object/array, return default
    if ((!data.isMappedObject() && !data.isArrayObject()) && effectiveSchema->has("default")) {
        return clone_value(effectiveSchema->at("default"));
    }

    // Otherwise return the input dictionary unchanged
    return data;
}

}  // namespace ps
