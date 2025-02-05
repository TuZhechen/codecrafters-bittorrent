#pragma once

#include "Command.hpp"
#include "../bencode/BencodeDecoder.hpp"

class DecodeCommand : public Command {
public:
    void execute(const std::string& input) override;
private:
    BencodeDecoder decoder;
}; 