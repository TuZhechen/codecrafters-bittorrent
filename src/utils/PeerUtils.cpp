#include "PeerUtils.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include "../protocol/PeerMessage.hpp"

std::pair<std::string, int> PeerUtils::parsePeerAddress(const std::string& peer_addr) {
    size_t colon_pos = peer_addr.find(':');
    if (colon_pos == std::string::npos) {
        throw std::runtime_error("Invalid peer address format. Expected: <ip>:<port>");
    }
    
    std::string ip = peer_addr.substr(0, colon_pos);
    int port = std::stoi(peer_addr.substr(colon_pos + 1));
    return {ip, port};
}

std::pair<std::string, int> PeerUtils::parsePeerAddress(const std::string& peers_data, int offset) {
    unsigned char ip_bytes[4];
    memcpy(ip_bytes, peers_data.c_str() + offset, 4);
    uint16_t port = static_cast<unsigned char>(peers_data[offset + 4]) << 8 | 
                   static_cast<unsigned char>(peers_data[offset + 5]);

    std::string ip_str = std::to_string(ip_bytes[0]) + "." + 
                        std::to_string(ip_bytes[1]) + "." + 
                        std::to_string(ip_bytes[2]) + "." + 
                        std::to_string(ip_bytes[3]);

    return {ip_str, port};
}

void PeerUtils::receiveMessage(unsigned char* msg_length_buf, char& msg_type, std::vector<uint8_t>& payload) {
    if (recv(sock, msg_length_buf, 4, 0) != 4) {
        throw std::runtime_error("Failed to receive message");
    }

    // Cast to unsigned char before shifting
    uint32_t msg_length = (msg_length_buf[0] << 24) | 
                         (msg_length_buf[1] << 16) |
                         (msg_length_buf[2] << 8) | 
                         msg_length_buf[3];

    // Add sanity check for message length
    if (msg_length > 16384 + 9) {  // Max block size (16KB) + metadata
        throw std::runtime_error("Message length too large: " + std::to_string(msg_length));
    }

    if (recv(sock, &msg_type, 1, 0) != 1) {
        throw std::runtime_error("Failed to receive message type");
    }


    payload.resize(msg_length - 1);
    // Skip payload receive if size is 0
    if (payload.size() > 0) {
        size_t total_received = 0;
        while (total_received < payload.size()) {
            ssize_t received = recv(sock, 
                                  payload.data() + total_received, 
                                  payload.size() - total_received, 
                                  0);
            if (received <= 0) {
                throw std::runtime_error("Failed to receive message payload");
            }
            total_received += received;
        }
    }
}

void PeerUtils::sendMessage(PeerMessageType msg_type, const std::vector<uint8_t>& payload) {
    
    auto msg = PeerMessage::create(msg_type, payload);
    auto serialized = msg.serialize();
    
    uint32_t length = msg.getLength();
    char length_buf[4] = {
        static_cast<char>((length >> 24) & 0xFF),
        static_cast<char>((length >> 16) & 0xFF),
        static_cast<char>((length >> 8) & 0xFF),
        static_cast<char>(length & 0xFF)
    };
    
    if (send(sock, length_buf, 4, 0) != 4) {
        throw std::runtime_error("Failed to send message length");
    }

    if (send(sock, serialized.data(), serialized.size(), 0) != serialized.size()) {
        throw std::runtime_error("Failed to send message body");
    }
}

void PeerUtils::addIntToPayload(std::vector<uint8_t>& payload, int value, int offset) {
    payload[offset] = (value >> 24) & 0xFF;
    payload[offset + 1] = (value >> 16) & 0xFF;
    payload[offset + 2] = (value >> 8) & 0xFF;
    payload[offset + 3] = value & 0xFF;
}
