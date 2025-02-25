#include "commands/DecodeCommand.hpp"
#include "commands/InfoCommand.hpp"
#include "commands/PeersCommand.hpp"
#include "commands/HandshakeCommand.hpp"
#include "commands/DownloadPieceCommand.hpp"
#include "commands/DownloadCommand.hpp"
#include "commands/MagnetParseCommand.hpp"
#include "commands/MagnetHandshakeCommand.hpp"
#include "manager/CommandManager.hpp"
#include <iostream>

CommandOptions parseCommandOptions(int argc, char* argv[]) {
    CommandOptions options;
    
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg[0] == '-') {  // This is an option
            if (i + 1 < argc) {  // Make sure we have a value after the option
                options.options[arg] = argv[i + 1];
                i++;  // Skip the next argument since it's the option value
            }
        } else {
            options.args.push_back(arg);
        }
    }
    
    return options;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <command> [options] <args>" << std::endl;
        return 1;
    }

    std::string command = argv[1];
    CommandOptions options = parseCommandOptions(argc, argv);

    CommandManager manager;
    manager.registerCommand("decode", std::make_unique<DecodeCommand>());
    manager.registerCommand("info", std::make_unique<InfoCommand>());
    manager.registerCommand("peers", std::make_unique<PeersCommand>());
    manager.registerCommand("handshake", std::make_unique<HandshakeCommand>());
    manager.registerCommand("download_piece", std::make_unique<DownloadPieceCommand>());
    manager.registerCommand("download", std::make_unique<DownloadCommand>());
    manager.registerCommand("magnet_parse", std::make_unique<MagnetParseCommand>());
    manager.registerCommand("magnet_handshake", std::make_unique<MagnetHandshakeCommand>());

    manager.executeCommand(command, options);

    return 0;
}
