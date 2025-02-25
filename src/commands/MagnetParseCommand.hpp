#pragma once

#include "Command.hpp"
#include <string>

class MagnetParseCommand : public Command {
public:
    void execute(const CommandOptions& options) override;
private:
    void displayMagnetInfo(const std::string& magnetLink);
}; 