#pragma once

#include "Command.hpp"
#include "../bencode/BencodeDecoder.hpp"
#include "../bencode/BencodeEncoder.hpp"
#include "../utils/SHA1.hpp"
#include "../utils/TorrentUtils.hpp"
#include <string>

class PeersCommand : public Command {
public:
    void execute(const CommandOptions& options) override;

private:
    void displayPeers(const std::string& peers_data);
}; 