#include <iostream>
#include <string>
#include "../include/P2PNode.h"

int main(int argc, char* argv[]) {
    uint16_t port = 0;

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--user" && i + 1 < argc) {
            std::string user = argv[i + 1];
            std::cout << "User set to: " << user << std::endl;
        }
        else if (std::string(argv[i]) == "--port" && i + 1 < argc) {
            try {
                port = std::stoi(argv[i + 1]);
            } catch (const std::exception& e) {
                std::cerr << "Error: wrong port format " << std::endl;
                return 1;
            }
        }
    }

    if (port == 0) {
        std::cerr << "Using: ./messenger --user <name> --port <number>" << std::endl;
        return 1;
    }

    P2PNode node;
    std::cout << "Running P2P node on port: " << port << "..." << std::endl;

    if (node.start(port)) {
        std::cout << "Node started successfully!" << std::endl;
    } else {
        std::cerr << "Node running Error." << std::endl;
    }

    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();
    return 0;
}