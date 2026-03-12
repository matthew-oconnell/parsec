#pragma once

#include <ps/pq/path_parser.h>
#include <string>
#include <vector>

namespace ps {
namespace pq {

// Text-level array manipulation that preserves comments and formatting.
// Instead of parsing → modifying → re-serializing (which loses comments),
// these functions locate the target array brackets in the raw text and
// splice in the new value directly.
class TextEditor {
public:
    // Prepend a value to the beginning of an array at the given path.
    // Returns the modified text with all comments and formatting preserved.
    static std::string prependToArray(const std::string& text,
                                       const std::vector<PathToken>& path,
                                       const std::string& valueText);

    // Append a value to the end of an array at the given path.
    // Returns the modified text with all comments and formatting preserved.
    static std::string appendToArray(const std::string& text,
                                      const std::vector<PathToken>& path,
                                      const std::string& valueText);
};

} // namespace pq
} // namespace ps
