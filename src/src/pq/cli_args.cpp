#include <ps/pq/cli_args.h>
#include <ps/cli_utils.h>
#include <stdexcept>
#include <string>

namespace ps {
namespace pq {

CliArgs::CliArgs(int argc, const char* argv[]) {
    if (argc < 2) {
        action_ = Action::HELP;
        return;
    }
    
    // First argument is the file path
    filePath_ = argv[1];
    
    // If only file is provided, default to print
    if (argc == 2) {
        action_ = Action::PRINT;
        return;
    }
    
    // Valid options for error suggestions
    static const std::vector<std::string> valid_options = {
        "--get", "-g",
        "--count",
        "--has",
        "--prepend",
        "--append",
        "--output", "-o",
        "--default", "-d",
        "--as-json"
    };
    
    // Parse flags
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--get" || arg == "-g") {
            action_ = Action::GET;
            if (i + 1 >= argc) {
                throw std::invalid_argument("--get requires a path argument");
            }
            path_ = argv[++i];
        }
        else if (arg == "--count") {
            action_ = Action::COUNT;
            if (i + 1 >= argc) {
                throw std::invalid_argument("--count requires a path argument");
            }
            path_ = argv[++i];
        }
        else if (arg == "--has") {
            action_ = Action::HAS;
            if (i + 1 >= argc) {
                throw std::invalid_argument("--has requires a path argument");
            }
            path_ = argv[++i];
        }
        else if (arg == "--default" || arg == "-d") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("--default requires a value argument");
            }
            defaultValue_ = argv[++i];
        }
        else if (arg == "--prepend") {
            action_ = Action::PREPEND;
            if (i + 1 >= argc) {
                throw std::invalid_argument("--prepend requires a path and a value argument");
            }
            path_ = argv[++i];
            if (i + 1 >= argc) {
                throw std::invalid_argument("--prepend requires a value argument after the path");
            }
            value_ = argv[++i];
        }
        else if (arg == "--append") {
            action_ = Action::APPEND;
            if (i + 1 >= argc) {
                throw std::invalid_argument("--append requires a path and a value argument");
            }
            path_ = argv[++i];
            if (i + 1 >= argc) {
                throw std::invalid_argument("--append requires a value argument after the path");
            }
            value_ = argv[++i];
        }
        else if (arg == "--output" || arg == "-o") {
            if (i + 1 >= argc) {
                throw std::invalid_argument("--output requires a file path argument");
            }
            outputPath_ = argv[++i];
        }
        else if (arg == "--as-json") {
            asJson_ = true;
        }
        else {
            std::string error_msg = cli_utils::create_unknown_arg_error(arg, valid_options);
            throw std::invalid_argument(error_msg);
        }
    }

    // Validate that mutation actions have an output path
    if (action_ == Action::PREPEND && outputPath_.empty()) {
        throw std::invalid_argument("--prepend requires --output <file> to specify the output file");
    }
    if (action_ == Action::APPEND && outputPath_.empty()) {
        throw std::invalid_argument("--append requires --output <file> to specify the output file");
    }
}

} // namespace pq
} // namespace ps
