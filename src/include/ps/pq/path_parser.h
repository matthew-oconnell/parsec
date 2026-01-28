#pragma once

#include <string>
#include <vector>

namespace ps {
namespace pq {

// Represents a single token in a path (either a key or an index)
class PathToken {
public:
    enum class Type { Key, Index, Wildcard };
    
    // Constructors
    static PathToken makeKey(const std::string& key);
    static PathToken makeIndex(int index);
    static PathToken makeWildcard();
    
    // Type checks
    bool isKey() const { return type_ == Type::Key; }
    bool isIndex() const { return type_ == Type::Index; }
    bool isWildcard() const { return type_ == Type::Wildcard; }
    
    // Accessors
    const std::string& asKey() const;
    int asIndex() const;
    
private:
    PathToken(Type type, const std::string& key, int index);
    
    Type type_;
    std::string key_;
    int index_;
};

// Parses slash-separated paths into tokens
class PathParser {
public:
    std::vector<PathToken> parse(const std::string& path);
    
private:
    bool isArrayIndex(const std::string& segment);
};

} // namespace pq
} // namespace ps
