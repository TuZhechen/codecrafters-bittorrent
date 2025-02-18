#pragma once
#include "PeerMessageType.hpp"
#include <vector>
#include <cstdint>

class PeerMessage {
public:
    // Constructors
    static PeerMessage create(PeerMessageType type, const std::vector<uint8_t>& payload = {});
    static PeerMessage parse(const std::vector<uint8_t>& data);
    
    // Getters
    PeerMessageType getType() const { return type; }
    const std::vector<uint8_t>& getPayload() const { return payload; }
    uint32_t getLength() const { return 1 + payload.size(); }  // message_id (1) + payload
    
    // Serialization
    std::vector<uint8_t> serialize() const;

private:
    PeerMessageType type;
    std::vector<uint8_t> payload;
}; 