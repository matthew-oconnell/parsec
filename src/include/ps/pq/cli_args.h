#pragma once

#include <string>
#include <optional>

namespace ps {
namespace pq {

// Parses command-line arguments for the pq tool
class CliArgs {
public:
    enum class Action {
        HELP,      // Show help message
        PRINT,     // Pretty-print the file (default)
        GET,       // Get a value at path
        COUNT,     // Count array elements at path
        HAS,       // Check if path exists
        PREPEND    // Prepend a value to an array at path
    };
    
    CliArgs(int argc, const char* argv[]);
    
    // Accessors
    Action getAction() const { return action_; }
    const std::string& getFilePath() const { return filePath_; }
    const std::string& getPath() const { return path_; }
    bool hasDefault() const { return defaultValue_.has_value(); }
    const std::string& getDefault() const { return *defaultValue_; }
    bool outputAsJson() const { return asJson_; }
    const std::string& getValue() const { return value_; }
    const std::string& getOutputPath() const { return outputPath_; }
    
private:
    Action action_ = Action::HELP;
    std::string filePath_;
    std::string path_;
    std::optional<std::string> defaultValue_;
    bool asJson_ = false;
    std::string value_;
    std::string outputPath_;
};

} // namespace pq
} // namespace ps
