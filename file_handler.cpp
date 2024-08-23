#include <fstream>
#include <iostream>
#include <sstream>

#include "OrderedVector.h"

std::vector<std::string> split_on_space(const std::string& line);

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Incorrect usage" << std::endl;
        std::cerr << "Usage example:" << std::endl;
        std::cerr << "\n\t./file_handler <input_file>.txt <output_file>.txt" << std::endl;
        return EXIT_FAILURE;
    }

    std::ifstream input_file(argv[1]);
    if (!input_file.is_open()) {
        std::cerr << "Could not open input file " << argv[1] << std::endl;
        return EXIT_FAILURE;
    }

    std::ofstream output_file(argv[2], std::ios::out);
    if (!output_file.is_open()) {
        std::cerr << "Could not open output file " << argv[2] << std::endl;
        return EXIT_FAILURE;
    }

    OrderedVector<int> v;
    std::string line;
    int line_count = 0;
    while (std::getline(input_file, line)) {
        std::vector<std::string> tokens = split_on_space(line);
        ++line_count;
        if (tokens.empty())
            break;

        if (tokens.front() == "INC") {
            if (tokens.size() != 2) {
                std::cerr << "Error on INC" << std::endl;
                std::cerr << "line " << line_count << ": " << line << std::endl;
                return EXIT_FAILURE;
            }

            int value = std::stoi(tokens[1]);
            v.push(value);
        } else if (tokens.front() == "REM") {
            if (tokens.size() != 2) {
                std::cerr << "Error on REM" << std::endl;
                std::cerr << "line " << line_count << ": " << line << std::endl;
                return EXIT_FAILURE;
            }

            int value = std::stoi(tokens[1]);
            v.remove(value);
        } else if (tokens.front() == "SUC") {
            if (tokens.size() != 2) {
                std::cerr << "Error on SUC" << std::endl;
                std::cerr << "line " << line_count << ": " << line << std::endl;
                return EXIT_FAILURE;
            }

            int value = std::stoi(tokens[1]);
            output_file << v.successor(value) << std::endl;
        } else if (tokens.front() == "IMP") {
            if (tokens.size() != 1) {
                std::cerr << "Error on IMP" << std::endl;
                std::cerr << "line " << line_count << ": " << line << std::endl;
                return EXIT_FAILURE;
            }

            output_file << v << std::endl;
        } else {
            if (!tokens.empty()) {
                std::cerr << "Undefined command " << tokens[0] << std::endl;
                std::cerr << "line " << line_count << ": " << line << std::endl;
            }
        }
    }

    input_file.close();
    output_file.close();
}

std::vector<std::string> split_on_space(const std::string& line) {
    std::stringstream ss(line);
    std::string token;
    std::vector<std::string> tokens;
    while (ss >> token)
        tokens.push_back(token);

    return tokens;
}

