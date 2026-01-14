#pragma once

#include <ps/dictionary.h>
#include <string>

namespace ps {

Dictionary parse_toml(const std::string& text);

// Serialize a Dictionary to a TOML-formatted string.
std::string dump_toml(const Dictionary& d);

}  // namespace ps
