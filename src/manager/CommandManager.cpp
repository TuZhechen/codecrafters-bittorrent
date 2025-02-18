#include "CommandManager.hpp"
#include <iostream>

void CommandManager::registerCommand(const std::string& name, std::unique_ptr<Command> command) {
    commands[name] = std::move(command);
}

void CommandManager::executeCommand(const std::string& name, const std::vector<std::string>& args) {
    auto it = commands.find(name);
    if (it != commands.end()) {
        try {
            it->second->execute(args);
        } catch (const std::exception& e) {
            std::cerr << "Error executing command: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "Unknown command: " << name << std::endl;
    }
}
