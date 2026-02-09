#include <ps/pq/path_parser.h>
#include <stdexcept>
#include <sstream>
#include <cctype>

namespace ps {
namespace pq {

// PathToken implementation
PathToken PathToken::makeKey(const std::string& key) {
    return PathToken(Type::Key, key, -1);
}

PathToken PathToken::makeIndex(int index) {
    return PathToken(Type::Index, "", index);
}

PathToken PathToken::makeWildcard() {
    return PathToken(Type::Wildcard, "", -1);
}

PathToken::PathToken(Type type, const std::string& key, int index)
    : type_(type), key_(key), index_(index) {}

const std::string& PathToken::asKey() const {
    if (type_ != Type::Key) {
        throw std::logic_error("PathToken is not a key");
    }
    return key_;
}

int PathToken::asIndex() const {
    if (type_ != Type::Index) {
        throw std::logic_error("PathToken is not an index");
    }
    return index_;
}

// PathParser implementation
std::vector<PathToken> PathParser::parse(const std::string& path) {
    if (path.empty()) {
        throw std::invalid_argument("Path cannot be empty");
    }
    
    // Auto-detect separator: prefer slash, fall back to dot
    char separator = '/';
    if (path.find('/') == std::string::npos && path.find('.') != std::string::npos) {
        separator = '.';
    }
    
    if (path[0] == separator) {
        std::string msg = "Path cannot start with ";
        msg += separator;
        throw std::invalid_argument(msg);
    }
    
    if (path[path.length() - 1] == separator) {
        std::string msg = "Path cannot end with ";
        msg += separator;
        throw std::invalid_argument(msg);
    }
    
    std::vector<PathToken> tokens;
    std::stringstream ss(path);
    std::string segment;
    
    while (std::getline(ss, segment, separator)) {
        if (segment.empty()) {
            throw std::invalid_argument("Path cannot contain empty segments (double slashes)");
        }
        
        // Check for wildcard
        if (segment == "*") {
            tokens.push_back(PathToken::makeWildcard());
        }
        // Check if segment is a valid non-negative integer
        else if (isArrayIndex(segment)) {
            int index = std::stoi(segment);
            tokens.push_back(PathToken::makeIndex(index));
        } else if (segment[0] == '-' && segment.length() > 1 && isArrayIndex(segment.substr(1))) {
            // Negative index - not allowed
            throw std::invalid_argument("Array indices must be non-negative");
        } else {
            // Treat as a key
            tokens.push_back(PathToken::makeKey(segment));
        }
    }
    
    return tokens;
}

bool PathParser::isArrayIndex(const std::string& segment) {
    if (segment.empty()) {
        return false;
    }
    
    // Check if all characters are digits
    for (char c : segment) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    
    return true;
}

} // namespace pq
} // namespace ps
