#pragma once

#include "Command.hpp"
#include "../bencode/BencodeParser.hpp"

class InfoCommand : public Command {
public:
    void execute(const std::string& input) override;
private:
    std::string readTorrentFile(const std::string& filepath);
    void displayTorrentInfo(const nlohmann::json& torrentData);
}; 