#include <ps/pq/output_formatter.h>
#include <sstream>

namespace ps {
namespace pq {

std::string OutputFormatter::formatRaw(const Dictionary& value) {
    // For scalars, output raw value without quotes
    try {
        // Try boolean first (before integer)
        bool b = value.asBool();
        return b ? "true" : "false";
    } catch (...) {}
    
    try {
        // Try double before integer to preserve decimal values
        double d = value.asDouble();
        // Check if it's actually an integer value
        if (d == static_cast<int>(d)) {
            return std::to_string(static_cast<int>(d));
        }
        std::ostringstream oss;
        oss << d;
        return oss.str();
    } catch (...) {}
    
    try {
        // Try string (output without quotes for shell use)
        return value.asString();
    } catch (...) {}
    
    // If it's an object or complex type, return JSON
    return value.dump(0, true);
}

std::string OutputFormatter::formatRaw(const std::vector<Dictionary>& values) {
    if (values.empty()) {
        return "";
    }
    
    std::ostringstream oss;
    for (size_t i = 0; i < values.size(); ++i) {
        oss << formatRaw(values[i]);
        if (i + 1 < values.size()) {
            oss << "\n";
        }
    }
    
    return oss.str();
}

std::string OutputFormatter::formatJson(const Dictionary& value) {
    // Use compact JSON format
    return value.dump(0, true);
}

std::string OutputFormatter::formatJson(const std::vector<Dictionary>& values) {
    if (values.empty()) {
        return "[]";
    }
    
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < values.size(); ++i) {
        oss << formatJson(values[i]);
        if (i + 1 < values.size()) {
            oss << ",";
        }
    }
    oss << "]";
    
    return oss.str();
}

} // namespace pq
} // namespace ps
