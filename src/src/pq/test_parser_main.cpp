// Simple test utility to validate PathParser from command line
// Usage: pq_test_parser "path/to/parse"
// Outputs: one token per line with type and value

#include <ps/pq/path_parser.h>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <path>\n";
        return 2;
    }
    
    std::string path = argv[1];
    
    try {
        ps::pq::PathParser parser;
        auto tokens = parser.parse(path);
        
        for (const auto& token : tokens) {
            if (token.isKey()) {
                std::cout << "key:" << token.asKey() << "\n";
            } else if (token.isIndex()) {
                std::cout << "index:" << token.asIndex() << "\n";
            } else if (token.isWildcard()) {
                std::cout << "wildcard:*\n";
            }
        }
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
