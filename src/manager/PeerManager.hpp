#pragma once
#include <string>
#include <vector>
#include <memory>
#include "../utils/PeerUtils.hpp"
#include "../utils/TorrentUtils.hpp"

class PeerManager {
public:
    PeerManager(const std::string& ip, int port, const std::string& info_hash);
    ~PeerManager();

    bool connect();
    bool downloadPiece(int index, int length, std::vector<uint8_t>& data);
    bool hasPiece(int index) const;
    void disconnect();
    bool isConnected() const { return peer_utils != nullptr; }
    std::string getPeerInfo() const { return ip + ":" + std::to_string(port); }

private:
    void processBitfield(const std::vector<uint8_t>& bitfield);
    std::unique_ptr<PeerUtils> peer_utils;
    std::string ip;
    int port;
    std::string info_hash;
    std::vector<bool> piece_availability;
};