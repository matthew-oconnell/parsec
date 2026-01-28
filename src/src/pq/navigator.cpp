#include <ps/pq/navigator.h>
#include <stdexcept>
#include <sstream>

namespace ps {
namespace pq {

Dictionary Navigator::navigate(const Dictionary& dict, const std::vector<PathToken>& tokens) {
    if (tokens.empty()) {
        return dict;
    }
    
    Dictionary current = dict;
    
    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto& token = tokens[i];
        
        if (token.isWildcard()) {
            throw std::invalid_argument("Wildcards require navigateWildcard(), not navigate()");
        }
        
        if (token.isKey()) {
            const std::string& key = token.asKey();
            if (!current.has(key)) {
                std::ostringstream oss;
                oss << "Key '" << key << "' not found";
                throw std::out_of_range(oss.str());
            }
            current = current.at(key);
        } else if (token.isIndex()) {
            int index = token.asIndex();
            
            // Check if current is an array
            if (current.size() == 0) {
                throw std::out_of_range("Cannot index into empty value");
            }
            
            // Try to access by index
            if (index < 0 || index >= static_cast<int>(current.size())) {
                std::ostringstream oss;
                oss << "Index " << index << " out of range (size: " << current.size() << ")";
                throw std::out_of_range(oss.str());
            }
            
            current = current.at(index);
        }
    }
    
    return current;
}

std::vector<Dictionary> Navigator::navigateWildcard(const Dictionary& dict, const std::vector<PathToken>& tokens) {
    std::vector<Dictionary> results;
    
    if (tokens.empty()) {
        results.push_back(dict);
        return results;
    }
    
    // Find first wildcard position
    size_t wildcardPos = tokens.size();
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].isWildcard()) {
            wildcardPos = i;
            break;
        }
    }
    
    // If no wildcard, use regular navigate
    if (wildcardPos == tokens.size()) {
        results.push_back(navigate(dict, tokens));
        return results;
    }
    
    // Navigate to the point before the wildcard
    std::vector<PathToken> beforeWildcard(tokens.begin(), tokens.begin() + wildcardPos);
    Dictionary current = beforeWildcard.empty() ? dict : navigate(dict, beforeWildcard);
    
    // Expand wildcard - iterate over all elements
    std::vector<Dictionary> wildcardResults;
    size_t size = static_cast<size_t>(current.size());
    for (size_t i = 0; i < size; ++i) {
        wildcardResults.push_back(current.at(static_cast<int>(i)));
    }
    
    // If there are tokens after the wildcard, navigate each result
    if (wildcardPos + 1 < tokens.size()) {
        std::vector<PathToken> afterWildcard(tokens.begin() + wildcardPos + 1, tokens.end());
        
        for (const auto& item : wildcardResults) {
            // Recursively handle remaining path (might have more wildcards)
            auto subResults = navigateWildcard(item, afterWildcard);
            results.insert(results.end(), subResults.begin(), subResults.end());
        }
    } else {
        // No more tokens after wildcard, return all items
        results = wildcardResults;
    }
    
    return results;
}

} // namespace pq
} // namespace ps
