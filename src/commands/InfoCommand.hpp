#pragma once

#include "Command.hpp"
#include "../bencode/BencodeDecoder.hpp"
#include "../bencode/BencodeEncoder.hpp"
#include "../utils/TorrentUtils.hpp"

class InfoCommand : public Command {
public:
    void execute(const CommandOptions& options) override;
private:
    void displayTorrentInfo(const nlohmann::json& torrentData);
}; 