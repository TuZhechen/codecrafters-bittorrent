#pragma once

#include "Command.hpp"
#include "../bencode/BencodeDecoder.hpp"
#include "../bencode/BencodeEncoder.hpp"
#include "../utils/SHA1.hpp"
#include "../utils/MagnetUtils.hpp"
#include "../utils/PeerUtils.hpp"
#include <string>

class MagnetHandshakeCommand : public Command {
public:
    void execute(const CommandOptions& options) override;

private:
    int sock = -1;
}; 