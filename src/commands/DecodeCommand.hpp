#pragma once

#include "Command.hpp"
#include "../bencode/BencodeParser.hpp"

class DecodeCommand : public Command {
public:
    void execute(const std::string& input) override;
private:
    BencodeParser parser;
}; 