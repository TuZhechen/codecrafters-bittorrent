#pragma once

#include "Command.hpp"
#include "../bencode/BencodeDecoder.hpp"
#include "../bencode/BencodeEncoder.hpp"
#include "../utils/SHA1.hpp"
#include "../utils/TorrentUtils.hpp"
#include <string>

class HandshakeCommand : public Command {
public:
    void execute(const CommandOptions& options) override;

private:
    int sock = -1;
    std::pair<std::string, int> parsePeerAddress(const std::string& peer_addr);
}; 