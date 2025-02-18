#include "PeerMessage.hpp"

PeerMessage PeerMessage::create(PeerMessageType type, const std::vector<uint8_t>& payload) {
    PeerMessage msg;
    msg.type = type;
    msg.payload = payload;
    return msg;
}

std::vector<uint8_t> PeerMessage::serialize() const {
    std::vector<uint8_t> result;
    result.push_back(static_cast<uint8_t>(type));
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
} 