#include <ps/parse.hpp>
#include <ps/simple_json.hpp>
#include <ps/ron.hpp>
#include <stdexcept>

namespace ps {

Value parse(const std::string& text) {
    try {
        return parse_json(text);
    } catch (const std::exception &e1) {
        std::string err1 = e1.what();
        try {
            return parse_ron(text);
        } catch (const std::exception &e2) {
            std::string err2 = e2.what();
            throw std::runtime_error(std::string("JSON parse error: ") + err1 + "\n---\nRON parse error: " + err2);
        }
    }
}

} // namespace ps