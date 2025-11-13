#include "ps/validate.h"

namespace ps {

// Helper: deep-copy a Value (Value uses shared_ptr for dicts so copy constructor is fine)
static Value clone_value(const Value &v) {
    return v;
}

// Recursively apply defaults from schema_node into target (which may be a dict or scalar)
static Value apply_defaults_to_value(const Value &dataVal, const Dictionary &schema_root, const Dictionary &schema_node);

static Dictionary apply_defaults_to_object(const Dictionary &input, const Dictionary &schema_root, const Dictionary &schema_node) {
    Dictionary out = input; // copy existing

    // properties
    const Dictionary* props = nullptr;
    auto it_props = schema_node.data.find("properties");
    if (it_props != schema_node.data.end()) props = it_props->second.isDict() ? it_props->second.asDict().get() : nullptr;

    if (props) {
        for (auto const &p : props->items()) {
            const std::string &k = p.first;
            const Value &propSchemaVal = p.second;
            const Dictionary* propSchema = propSchemaVal.isDict() ? propSchemaVal.asDict().get() : nullptr;

            // If key present in input, recursively apply defaults into it
            if (out.has(k)) {
                if (propSchema) {
                    Value existing = out[k];
                    Value withDefaults = apply_defaults_to_value(existing, schema_root, *propSchema);
                    out[k] = withDefaults;
                }
            } else {
                // Not present: if schema has a default, use it; else if propSchema is an object with nested defaults, we may
                // create an empty object and apply nested defaults if there are defaults deeper in the schema.
                if (propSchema && propSchema->data.find("default") != propSchema->data.end()) {
                    out[k] = clone_value(propSchema->data.at("default"));
                } else if (propSchema && propSchema->data.find("type") != propSchema->data.end() && propSchema->data.at("type").isString() && propSchema->data.at("type").asString() == "object") {
                    // If nested object may have defaults inside, create an empty object and apply nested defaults
                    Dictionary empty;
                    Dictionary nested = apply_defaults_to_object(empty, schema_root, *propSchema);
                    if (!nested.empty()) out[k] = Value(nested);
                }
            }
        }
    }

    // additionalProperties: if schema object with default, and keys in input not covered by properties, ensure defaults applied
    auto it_add = schema_node.data.find("additionalProperties");
    const Dictionary* addSchema = nullptr;
    if (it_add != schema_node.data.end() && it_add->second.isDict()) addSchema = it_add->second.asDict().get();

    if (addSchema) {
        for (auto const &pd : out.items()) {
            const std::string &k = pd.first;
            if (props && props->data.find(k) != props->data.end()) continue; // covered by properties
            // apply defaults into this additional property
            Value existing = pd.second;
            Value withDefaults = apply_defaults_to_value(existing, schema_root, *addSchema);
            out[k] = withDefaults;
        }
    }

    return out;
}

static Value apply_defaults_to_value(const Value &dataVal, const Dictionary &schema_root, const Dictionary &schema_node) {
    // If schema_node has a 'default' and dataVal is null/empty, return default
    if ((dataVal.isNull() || (!dataVal.isDict() && !dataVal.isList())) ) {
        auto it_def = schema_node.data.find("default");
        if (it_def != schema_node.data.end()) return clone_value(it_def->second);
    }

    // If schema type is object, recurse
    auto it_type = schema_node.data.find("type");
    if (it_type != schema_node.data.end() && it_type->second.isString() && it_type->second.asString() == "object") {
        Dictionary inObj;
        if (dataVal.isDict()) inObj = *dataVal.asDict();
        Dictionary outObj = apply_defaults_to_object(inObj, schema_root, schema_node);
        return Value(outObj);
    }

    // If type is array and default exists at the array level but element defaults are not handled here
    if (it_type != schema_node.data.end() && it_type->second.isString() && it_type->second.asString() == "array") {
        if (!dataVal.isList()) {
            auto it_def = schema_node.data.find("default");
            if (it_def != schema_node.data.end()) return clone_value(it_def->second);
        } else {
            // For array elements, if items is a schema and elements are objects, apply nested defaults per element
            auto it_items = schema_node.data.find("items");
            if (it_items != schema_node.data.end() && it_items->second.isDict() && dataVal.isList()) {
                const Dictionary* itemSchema = it_items->second.asDict().get();
                Value::list_t outList;
                for (auto const &el : dataVal.asList()) {
                    outList.push_back(apply_defaults_to_value(el, schema_root, *itemSchema));
                }
                return Value(outList);
            }
        }
    }

    // Otherwise return dataVal unchanged
    return dataVal;
}

Dictionary setDefaults(const Dictionary& data, const Dictionary& schema) {
    // Use schema as root for $ref resolution in future work; for now pass schema through
    Value vdata;
    if (data.scalar) vdata = *data.scalar;
    else vdata = Value(data);

    // Top-level: if schema declares type object, apply object defaults; otherwise if default exists, use it
    auto it_type = schema.data.find("type");
    if (it_type != schema.data.end() && it_type->second.isString() && it_type->second.asString() == "object") {
        Dictionary inObj;
        if (vdata.isDict()) inObj = *vdata.asDict();
        Dictionary outObj = apply_defaults_to_object(inObj, schema, schema);
        return outObj;
    }

    // If schema has a top-level default and data is empty, return scalar with default
    if ((!vdata.isDict() && !vdata.isList()) && schema.data.find("default") != schema.data.end()) {
        Dictionary out; out = schema.data.at("default"); return out;
    }

    // Otherwise return the input dictionary unchanged
    return data;
}

} // namespace ps
