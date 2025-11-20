#pragma once

#include <ps/dictionary.h>
#include <string>

namespace ps {

Dictionary parse_toml(const std::string& text);

}  // namespace ps
