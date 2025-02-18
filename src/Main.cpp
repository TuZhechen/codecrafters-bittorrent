#include "commands/DecodeCommand.hpp"
#include "commands/InfoCommand.hpp"
#include "commands/PeersCommand.hpp"
#include "commands/HandshakeCommand.hpp"
#include "manager/CommandManager.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <command> <args>" << std::endl;
        return 1;
    }

    std::string command = argv[1];
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) {
        args.push_back(argv[i]);
    }

    CommandManager manager;
    manager.registerCommand("decode", std::make_unique<DecodeCommand>());
    manager.registerCommand("info", std::make_unique<InfoCommand>());
    manager.registerCommand("peers", std::make_unique<PeersCommand>());
    manager.registerCommand("handshake", std::make_unique<HandshakeCommand>());

    manager.executeCommand(command, args);

    return 0;
}
