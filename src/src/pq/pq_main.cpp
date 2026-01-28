// pq - Path Query tool for extracting values from config files
// A shell-friendly alternative to jq

#include <ps/parsec.h>
#include <ps/pq/path_parser.h>
#include <ps/pq/navigator.h>
#include <ps/pq/cli_args.h>
#include <ps/pq/output_formatter.h>

#include <iostream>
#include <fstream>
#include <sstream>

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void showHelp() {
    std::cout << "pq - Path Query tool for config files\n\n";
    std::cout << "Usage:\n";
    std::cout << "  pq <file> --get <path> [--default <value>] [--as-json]\n";
    std::cout << "  pq <file> --count <path>\n";
    std::cout << "  pq <file> --has <path>\n";
    std::cout << "  pq <file>\n\n";
    std::cout << "Actions:\n";
    std::cout << "  --get, -g <path>     Extract value at path\n";
    std::cout << "  --count <path>       Count array elements at path\n";
    std::cout << "  --has <path>         Check if path exists (exit 0/1)\n";
    std::cout << "  (default)            Pretty-print entire file\n\n";
    std::cout << "Options:\n";
    std::cout << "  --default, -d <val>  Default value if path not found\n";
    std::cout << "  --as-json            Output as JSON instead of raw\n\n";
    std::cout << "Path syntax:\n";
    std::cout << "  Keys separated by /: server/port\n";
    std::cout << "  Array indices:       users/0/name\n";
    std::cout << "  Wildcards:           users/*/email\n";
    std::cout << "  Spaces in keys:      \"server config/port number\"\n\n";
    std::cout << "Examples:\n";
    std::cout << "  pq config.json --get server/port\n";
    std::cout << "  pq config.yaml --get timeout --default 30\n";
    std::cout << "  pq data.toml --count users\n";
    std::cout << "  pq settings.ron --has debug/enabled\n";
    std::cout << "  pq config.json --get \"mesh adaptation/starting mesh complexity\"\n";
}

int main(int argc, const char* argv[]) {
    try {
        // Parse command-line arguments
        ps::pq::CliArgs args(argc, argv);
        
        if (args.getAction() == ps::pq::CliArgs::Action::HELP) {
            showHelp();
            return 0;
        }
        
        // Read and parse the file
        std::string content = readFile(args.getFilePath());
        ps::Dictionary data = ps::parse(content, false, args.getFilePath());
        
        ps::pq::PathParser pathParser;
        ps::pq::Navigator navigator;
        ps::pq::OutputFormatter formatter;
        
        switch (args.getAction()) {
            case ps::pq::CliArgs::Action::PRINT: {
                // Pretty-print the entire file
                std::cout << data.dump(4, false) << "\n";
                return 0;
            }
            
            case ps::pq::CliArgs::Action::GET: {
                // Extract value at path
                auto tokens = pathParser.parse(args.getPath());
                
                try {
                    // Check if path contains wildcards
                    bool hasWildcard = false;
                    for (const auto& token : tokens) {
                        if (token.isWildcard()) {
                            hasWildcard = true;
                            break;
                        }
                    }
                    
                    if (hasWildcard) {
                        // Use wildcard navigation
                        auto results = navigator.navigateWildcard(data, tokens);
                        
                        if (results.empty() && args.hasDefault()) {
                            std::cout << args.getDefault() << "\n";
                        } else if (args.outputAsJson()) {
                            std::cout << formatter.formatJson(results) << "\n";
                        } else {
                            std::cout << formatter.formatRaw(results) << "\n";
                        }
                    } else {
                        // Use regular navigation
                        auto result = navigator.navigate(data, tokens);
                        
                        if (args.outputAsJson()) {
                            std::cout << formatter.formatJson(result) << "\n";
                        } else {
                            std::cout << formatter.formatRaw(result) << "\n";
                        }
                    }
                } catch (const std::out_of_range&) {
                    if (args.hasDefault()) {
                        std::cout << args.getDefault() << "\n";
                        return 0;
                    }
                    throw;
                }
                
                return 0;
            }
            
            case ps::pq::CliArgs::Action::COUNT: {
                // Count array elements
                auto tokens = pathParser.parse(args.getPath());
                auto result = navigator.navigate(data, tokens);
                std::cout << result.size() << "\n";
                return 0;
            }
            
            case ps::pq::CliArgs::Action::HAS: {
                // Check if path exists
                auto tokens = pathParser.parse(args.getPath());
                try {
                    navigator.navigate(data, tokens);
                    return 0;  // Exists
                } catch (const std::out_of_range&) {
                    return 1;  // Does not exist
                }
            }
            
            case ps::pq::CliArgs::Action::HELP:
                // Already handled above
                return 0;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
