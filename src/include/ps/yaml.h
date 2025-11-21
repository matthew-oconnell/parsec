#pragma once

#include <ps/dictionary.h>
#include <string>

namespace ps {

Dictionary parse_yaml(const std::string& text);

// Serialize a Dictionary to a YAML-formatted string.
std::string dump_yaml(const Dictionary& dict);

}  // namespace ps
