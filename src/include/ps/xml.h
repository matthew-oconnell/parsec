#pragma once

#include <ps/dictionary.h>
#include <string>

namespace ps {

// Parse a simple XML document into a Dictionary.
// Top-level Dictionary maps the root element name to its object/value.
Dictionary parse_xml(const std::string& text);

}  // namespace ps
