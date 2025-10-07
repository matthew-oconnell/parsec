include "json.hpp"; using namespace json;

class DictionaryObject {
public:
    DictionaryObject(std::string path) {
        this->loadFromJson(path);
    }

    void loadFromJson(const std::string& path) {
        // parse JSON file and populate member variables
    }
};