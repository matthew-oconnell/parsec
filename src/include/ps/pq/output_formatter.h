#pragma once

#include <ps/parsec.h>
#include <string>
#include <vector>

namespace ps {
namespace pq {

// Formats Dictionary values for output
class OutputFormatter {
public:
    // Format a single value as raw text (for shell consumption)
    std::string formatRaw(const Dictionary& value);
    
    // Format multiple values as raw text (one per line)
    std::string formatRaw(const std::vector<Dictionary>& values);
    
    // Format a single value as JSON
    std::string formatJson(const Dictionary& value);
    
    // Format multiple values as JSON array
    std::string formatJson(const std::vector<Dictionary>& values);
};

} // namespace pq
} // namespace ps
