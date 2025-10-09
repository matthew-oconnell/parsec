#include "ps/validate.hpp"
#include <sstream>

namespace ps {

static std::string type_name(const Dictionary::TYPE t) {
    switch (t) {
        case Dictionary::String: return "string";
        case Dictionary::Integer: return "integer";
        case Dictionary::Double: return "double";
        case Dictionary::Boolean: return "boolean";
        case Dictionary::Object: return "object";
        case Dictionary::IntArray: return "int[]";
        case Dictionary::DoubleArray: return "double[]";
        case Dictionary::StringArray: return "string[]";
        case Dictionary::BoolArray: return "bool[]";
        case Dictionary::ObjectArray: return "object[]";
        default: return "null";
    }
}

std::optional<std::string> validate(const Dictionary& data, const Dictionary& schema) {
    // schema is expected to be a mapping where each key describes a property
    // and the value is an object that may contain "type", "required", "minimum", "maximum"

    for (auto const &p: schema.items()) {
        const std::string &key = p.first;
        const Value &specVal = p.second;
        if (!specVal.isDict()) return std::optional<std::string>("schema entry for '" + key + "' must be an object");
        const Dictionary &spec = *specVal.asDict();

        bool required = false;
        if (spec.data.find("required") != spec.data.end() && spec.data.at("required").isBool()) {
            required = spec.data.at("required").asBool();
        }

        auto it = data.data.find(key);
        if (it == data.data.end()) {
            if (required) return std::optional<std::string>("missing required property '" + key + "'");
            continue;
        }

        const Value &val = it->second;
        // type check
        if (spec.data.find("type") != spec.data.end() && spec.data.at("type").isString()) {
            std::string want = spec.data.at("type").asString();
            Dictionary::TYPE wantType = Dictionary::Null;
            if (want == "string") wantType = Dictionary::String;
            else if (want == "integer") wantType = Dictionary::Integer;
            else if (want == "double") wantType = Dictionary::Double;
            else if (want == "boolean") wantType = Dictionary::Boolean;
            else if (want == "object") wantType = Dictionary::Object;

            Dictionary::TYPE got = Dictionary::Null;
            if (val.isString()) got = Dictionary::String;
            else if (val.isInt()) got = Dictionary::Integer;
            else if (val.isDouble()) got = Dictionary::Double;
            else if (val.isBool()) got = Dictionary::Boolean;
            else if (val.isDict()) got = Dictionary::Object;

            if (got != wantType) {
                std::ostringstream ss; ss << "property '" << key << "' expected type " << want << " but got " << type_name(got);
                return std::optional<std::string>(ss.str());
            }
        }

        // numeric constraints
        if ((val.isInt() || val.isDouble()) && spec.data.find("minimum") != spec.data.end()) {
            double minv = 0.0;
            if (spec.data.at("minimum").isInt()) minv = spec.data.at("minimum").asInt();
            else if (spec.data.at("minimum").isDouble()) minv = spec.data.at("minimum").asDouble();
            double got = val.isInt() ? static_cast<double>(val.asInt()) : val.asDouble();
            if (got < minv) {
                std::ostringstream ss; ss << "property '" << key << "' value " << got << " below minimum " << minv;
                return std::optional<std::string>(ss.str());
            }
        }
        if ((val.isInt() || val.isDouble()) && spec.data.find("maximum") != spec.data.end()) {
            double maxv = 0.0;
            if (spec.data.at("maximum").isInt()) maxv = spec.data.at("maximum").asInt();
            else if (spec.data.at("maximum").isDouble()) maxv = spec.data.at("maximum").asDouble();
            double got = val.isInt() ? static_cast<double>(val.asInt()) : val.asDouble();
            if (got > maxv) {
                std::ostringstream ss; ss << "property '" << key << "' value " << got << " above maximum " << maxv;
                return std::optional<std::string>(ss.str());
            }
        }
    }
    return std::nullopt;
}

} // namespace ps
