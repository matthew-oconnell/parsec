#include <ps/pq/cli_args.h>
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
        else if (arg == "--as-json") {
            asJson_ = true;
        }
        else {
            throw std::invalid_argument("Unknown argument: " + arg);
        }
    }
}

} // namespace pq
} // namespace ps
