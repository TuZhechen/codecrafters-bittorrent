#pragma once

#include "Command.hpp"
#include "../bencode/BencodeDecoder.hpp"

class DecodeCommand : public Command {
public:
    void execute(const std::vector<std::string>& args) override;
private:
    BencodeDecoder decoder;
}; 