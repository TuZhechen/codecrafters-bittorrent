#pragma once

#include "Command.hpp"
#include "../bencode/BencodeDecoder.hpp"
#include "../bencode/BencodeEncoder.hpp"

class InfoCommand : public Command {
public:
    void execute(const std::vector<std::string>& args) override;
private:
    std::string readTorrentFile(const std::string& filepath);
    void displayTorrentInfo(const nlohmann::json& torrentData);
}; 