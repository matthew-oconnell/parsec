#pragma once

#include <ps/pq/path_parser.h>
#include <ps/parsec.h>
#include <vector>

namespace ps {
namespace pq {

// Navigates through a Dictionary using parsed path tokens
class Navigator {
public:
    // Navigate to a single value using path tokens
    // Throws std::out_of_range if path doesn't exist
    Dictionary navigate(const Dictionary& dict, const std::vector<PathToken>& tokens);
    
    // Navigate with wildcard support - returns multiple results
    // Returns empty vector if no matches found
    std::vector<Dictionary> navigateWildcard(const Dictionary& dict, const std::vector<PathToken>& tokens);
};

} // namespace pq
} // namespace ps
