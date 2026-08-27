#include "elf_inspector.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    std::string target_path = "/proc/self/exe";

    if (argc > 1) {
        target_path = argv[1];
    }

    std::cout << "========================================================================\n";
    std::cout << " Lightweight ELF64 Binary Inspector (C++20)\n";
    std::cout << " Target File: " << target_path << "\n";
    std::cout << "========================================================================\n\n";

    try {
        auto inspector = sys::elf::Elf64Inspector::from_file(target_path);
        if (!inspector.is_valid()) {
            std::cerr << "Error: " << target_path << " is not a valid 64-bit ELF binary.\n";
            return 1;
        }

        inspector.print_summary(std::cout);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception during ELF inspection: " << e.what() << "\n";
        return 1;
    }
}
