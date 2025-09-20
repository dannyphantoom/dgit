#include "dgit/commands.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    dgit::CLI cli;

    try {
        return cli.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}

// Simple test function to demonstrate basic functionality
void test_basic_functionality() {
    std::cout << "dgit - Git implementation in C++\n";
    std::cout << "This is a basic working version with core functionality.\n";
    std::cout << "Available commands: init, add, commit, status, log, branch, checkout\n";
    std::cout << "\nTo initialize a repository, run: dgit init\n";
}
