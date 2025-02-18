#pragma once

#include "Command.hpp"
#include "../bencode/BencodeDecoder.hpp"
#include "../bencode/BencodeEncoder.hpp"
#include "../utils/SHA1.hpp"
#include <string>

class HandshakeCommand : public Command {
public:
    void execute(const std::vector<std::string>& args) override;

private:
    std::string readTorrentFile(const std::string& filepath);
    void performHandshake(const std::string& peer_addr, const std::string& info_hash);
    std::pair<std::string, int> parsePeerAddress(const std::string& peer_addr);
}; 