#pragma once

#include <ps/dictionary.h>
#include <string>

namespace ps {

Dictionary parse_ron(const std::string& text);

// Serialize a Dictionary to a RON-formatted string.
std::string dump_ron(const Dictionary& d);

}  // namespace ps
