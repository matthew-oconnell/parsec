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
    std::cout << "  --prepend <path> <v> Prepend value to array at path\n";
    std::cout << "  --append <path> <v>  Append value to array at path\n";
    std::cout << "  (default)            Pretty-print entire file\n\n";
    std::cout << "Options:\n";
    std::cout << "  --default, -d <val>  Default value if path not found\n";
    std::cout << "  --output, -o <file>  Output file (required for mutations)\n";
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
    std::cout << "  pq users.json --prepend users '{\"name\":\"Alice\"}' --output updated.json\n";
    std::cout << "  pq users.json --append users '{\"name\":\"Bob\"}' --output updated.json\n";
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
            
            case ps::pq::CliArgs::Action::PREPEND: {
                // Prepend a value to an array at path
                auto tokens = pathParser.parse(args.getPath());
                auto& target = navigator.navigateMutable(data, tokens);

                if (target.size() == 0 && !target.has("__dummy__")) {
                    // Could be empty object or empty array — treat as array
                }

                int n = target.size();
                // Shift existing elements up by one
                for (int i = n; i > 0; --i) {
                    target[i] = target[i - 1];
                }

                // Parse the value and insert at index 0
                ps::Dictionary newValue = ps::parse(args.getValue());
                target[0] = newValue;

                // Write to output file
                std::ofstream out(args.getOutputPath());
                if (!out.is_open()) {
                    throw std::runtime_error("Failed to open output file: " + args.getOutputPath());
                }
                out << data.dump(4, false) << "\n";
                return 0;
            }

            case ps::pq::CliArgs::Action::APPEND: {
                // Append a value to an array at path
                auto tokens = pathParser.parse(args.getPath());
                auto& target = navigator.navigateMutable(data, tokens);

                int n = target.size();
                ps::Dictionary newValue = ps::parse(args.getValue());
                target[n] = newValue;

                // Write to output file
                std::ofstream out(args.getOutputPath());
                if (!out.is_open()) {
                    throw std::runtime_error("Failed to open output file: " + args.getOutputPath());
                }
                out << data.dump(4, false) << "\n";
                return 0;
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
