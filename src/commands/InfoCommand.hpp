#pragma once

#include "Command.hpp"
#include "../bencode/BencodeDecoder.hpp"
#include "../bencode/BencodeEncoder.hpp"

class InfoCommand : public Command {
public:
    void execute(const std::string& input) override;
private:
    std::string readTorrentFile(const std::string& filepath);
    void displayTorrentInfo(const nlohmann::json& torrentData);
}; 