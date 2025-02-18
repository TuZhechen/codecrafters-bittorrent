#pragma once

#include "Command.hpp"
#include "../bencode/BencodeDecoder.hpp"

class DecodeCommand : public Command {
public:
    void execute(const CommandOptions& options) override;
private:
    BencodeDecoder decoder;
}; 