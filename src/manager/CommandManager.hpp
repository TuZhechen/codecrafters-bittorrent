#pragma once

#include "../commands/Command.hpp"
#include <map>
#include <memory>

class CommandManager {
public:
    void registerCommand(const std::string& name, std::unique_ptr<Command> command);
    void executeCommand(const std::string& name, const std::string& input);

private:
    std::map<std::string, std::unique_ptr<Command>> commands;
}; 