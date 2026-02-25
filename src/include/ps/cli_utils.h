#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <limits>

namespace ps {
namespace cli_utils {

// Compute Levenshtein distance between two strings
// This measures how many single-character edits are needed to change one string into another
inline int levenshtein_distance(const std::string& s1, const std::string& s2) {
    const size_t m = s1.size();
    const size_t n = s2.size();
    
    // Create a matrix to store distances
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));
    
    // Initialize base cases
    for (size_t i = 0; i <= m; ++i) {
        dp[i][0] = static_cast<int>(i);
    }
    for (size_t j = 0; j <= n; ++j) {
        dp[0][j] = static_cast<int>(j);
    }
    
    // Fill in the matrix
    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            if (s1[i - 1] == s2[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1];
            } else {
                dp[i][j] = 1 + std::min({
                    dp[i - 1][j],      // deletion
                    dp[i][j - 1],      // insertion
                    dp[i - 1][j - 1]   // substitution
                });
            }
        }
    }
    
    return dp[m][n];
}

// Find the most similar option from a list of valid options
// Returns the suggestion and the distance, or empty string if no good match
inline std::string suggest_similar_option(const std::string& unknown_arg, 
                                         const std::vector<std::string>& valid_options) {
    if (valid_options.empty()) {
        return "";
    }
    
    int min_distance = std::numeric_limits<int>::max();
    std::string best_match;
    
    for (const auto& option : valid_options) {
        int dist = levenshtein_distance(unknown_arg, option);
        if (dist < min_distance) {
            min_distance = dist;
            best_match = option;
        }
    }
    
    // Only suggest if the distance is reasonable (within 3 edits or 40% of the string length)
    int threshold = std::max(3, static_cast<int>(unknown_arg.length() * 0.4));
    if (min_distance <= threshold) {
        return best_match;
    }
    
    return "";
}

// Create an error message for an unknown argument with a helpful suggestion
inline std::string create_unknown_arg_error(const std::string& unknown_arg,
                                           const std::vector<std::string>& valid_options) {
    std::string error = "Unknown argument: " + unknown_arg;
    
    std::string suggestion = suggest_similar_option(unknown_arg, valid_options);
    if (!suggestion.empty()) {
        error += "\n  Did you mean '" + suggestion + "'?";
    }
    
    return error;
}

} // namespace cli_utils
} // namespace ps
