#pragma once
#include "Command.hpp"
#include "../bencode/BencodeDecoder.hpp"
#include "../bencode/BencodeEncoder.hpp"
#include "../protocol/PeerMessageType.hpp"
#include "../utils/TorrentUtils.hpp"
#include "../utils/PeerUtils.hpp"
#include "../utils/SHA1.hpp"
#include <string>

class DownloadPieceCommand : public Command {
public:
    void execute(const CommandOptions& options) override;
}; 