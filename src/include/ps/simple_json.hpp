// Simple in-repo JSON parser API (moved under src/include)
#pragma once

#include <ps/dictionary.hpp>
#include <string>

namespace ps {

Value parse_json(const std::string& text);

namespace json_literals {
	inline Dictionary operator"" _dict(const char* s, std::size_t len) {
		return *parse_json(std::string(s, len)).asDict();
	}
	inline Value operator"" _json(const char* s, std::size_t len) {
		return parse_json(std::string(s, len));
	}
}

} // namespace ps
