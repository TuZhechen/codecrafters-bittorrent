#pragma once

#include "Command.hpp"
#include "../bencode/BencodeDecoder.hpp"
#include "../bencode/BencodeEncoder.hpp"
#include "../utils/SHA1.hpp"
#include <string>

class PeersCommand : public Command {
public:
    void execute(const std::vector<std::string>& args) override;

private:
    std::string readTorrentFile(const std::string& filepath);
    std::string makeTrackerRequest(const std::string& announce_url, 
                                 const std::string& info_hash,
                                 int64_t length);
    void displayPeers(const std::string& peers_data);
    std::string urlEncode(const unsigned char* data, size_t len);
}; 