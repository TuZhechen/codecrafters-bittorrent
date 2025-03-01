#pragma once
#include "Command.hpp"
#include "../bencode/Bencode.hpp"
#include "../protocol/PeerMessageType.hpp"
#include "../utils/MagnetUtils.hpp"
#include "../utils/PeerUtils.hpp"
#include "../utils/SHA1.hpp"
#include <string>

class MagnetDownloadPieceCommand : public Command {
public:
    void execute(const CommandOptions& options) override;
}; 