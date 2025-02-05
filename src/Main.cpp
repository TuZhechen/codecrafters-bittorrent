#include "commands/DecodeCommand.hpp"
#include "commands/InfoCommand.hpp"
#include "manager/CommandManager.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <command> <input>" << std::endl;
        return 1;
    }

    std::string command = argv[1];
    std::string input = argc > 2 ? argv[2] : "";

    CommandManager manager;
    manager.registerCommand("decode", std::make_unique<DecodeCommand>());
    manager.registerCommand("info", std::make_unique<InfoCommand>());

    manager.executeCommand(command, input);

    return 0;
}
