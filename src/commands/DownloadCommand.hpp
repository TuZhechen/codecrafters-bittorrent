#pragma once
#include "Command.hpp"
#include "../bencode/BencodeDecoder.hpp"
#include "../bencode/BencodeEncoder.hpp"
#include "../protocol/PeerMessageType.hpp"
#include "../utils/TorrentUtils.hpp"
#include "../utils/PeerUtils.hpp"
#include "../utils/SHA1.hpp"
#include "../manager/PieceManager.hpp"
#include "../manager/PeerManager.hpp"
#include <memory>
#include <queue>

class DownloadCommand : public Command {
public:
    void execute(const CommandOptions& options) override;

private:
    // Core initialization
    void connectToPeers(const std::string& announce_url);
    
    // Download management
    void downloadAllPieces();
    void downloadPieceFromPeer(int piece_index, PeerManager& peer);
    
    // State
    std::unique_ptr<PieceManager> piece_manager;
    std::vector<std::unique_ptr<PeerManager>> peers;
    
    // Only keep info_hash as it's needed for peer connections
    std::string info_hash;
};
