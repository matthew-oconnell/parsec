// Simple CLI for parsing a JSON file using parsec
#include <iostream>
#include <fstream>
#include <string>
#include <ps/simple_json.hpp>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: parsec_cli <file.json>\n";
        return 2;
    }
    const std::string path = argv[1];
    std::ifstream in(path);
    if (!in) {
        std::cerr << "error: cannot open file: " << path << "\n";
        return 2;
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    try {
        auto v = ps::parse_json(content);
        std::cout << "OK: parsed JSON; value: " << v.to_string() << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "parse error: " << e.what() << "\n";
        return 1;
    }
}
