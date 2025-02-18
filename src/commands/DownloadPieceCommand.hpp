#pragma once
#include "Command.hpp"
#include "../bencode/BencodeDecoder.hpp"
#include "../bencode/BencodeEncoder.hpp"
#include "../protocol/PeerMessageType.hpp"
#include "../utils/TorrentUtils.hpp"
#include "../utils/SHA1.hpp"
#include <string>

class DownloadPieceCommand : public Command {
public:
    void execute(const CommandOptions& options) override;
private:
    int sock = -1;  // Socket file descriptor
    void receiveMessage(char* msg_length_buf, char& msg_type, std::vector<uint8_t>& payload);
    void sendMessage(PeerMessageType msg_type, const std::vector<uint8_t>& payload);
    void addIntToPayload(std::vector<uint8_t>& payload, int value, int offset);
}; 