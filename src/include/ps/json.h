#pragma once

#include <ps/dictionary.h>
#include <string>

namespace ps {

Dictionary parse_json(const std::string& text);

namespace json_literals {
    inline Dictionary operator"" _json(const char* s, std::size_t len) {
        return parse_json(std::string(s, len));
    }
}

}  // namespace ps
