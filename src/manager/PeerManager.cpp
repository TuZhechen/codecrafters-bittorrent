#include "PeerManager.hpp"
#include "../protocol/PeerMessage.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdexcept>
#include <iostream>
#include <queue>

PeerManager::PeerManager(const std::string& ip, int port, const std::string& info_hash)
    : ip(ip), port(port), info_hash(info_hash), peer_utils(nullptr) {
}

PeerManager::~PeerManager() {
    disconnect();
}

bool PeerManager::connect() {
    try {
        // Create socket
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            std::cerr << "Failed to create socket for " << ip << std::endl;
            return false;
        }

        // Connect to peer
        struct sockaddr_in peer_addr;
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, ip.c_str(), &peer_addr.sin_addr) <= 0) {
            close(sock);
            return false;
        }

        if (::connect(sock, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0) {
            close(sock);
            return false;
        }

        // Initialize PeerUtils
        peer_utils = std::make_unique<PeerUtils>(sock);

        // Perform handshake
        TorrentUtils::performHandshake(sock, info_hash);

        // Receive and process bitfield
        unsigned char msg_length_buf[4];
        char msg_type;
        std::vector<uint8_t> payload;
        
        peer_utils->receiveMessage(msg_length_buf, msg_type, payload);

        if (msg_type != static_cast<char>(PeerMessageType::BITFIELD)) {
            disconnect();
            return false;
        }
        
        processBitfield(payload);

        // Send interested message
        peer_utils->sendMessage(PeerMessageType::INTERESTED, {});

        // Wait for unchoke
        peer_utils->receiveMessage(msg_length_buf, msg_type, payload);

        if (msg_type != static_cast<char>(PeerMessageType::UNCHOKE)) {
            disconnect();
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Connection failed: " << e.what() << std::endl;
        disconnect();
        return false;
    }
}

bool PeerManager::downloadPiece(int index, int length, std::vector<uint8_t>& data) {
    if (!peer_utils || !hasPiece(index)) {
        std::cout << "Peer " << getPeerInfo() << " can't download piece " 
                  << index << " (connected=" << (peer_utils != nullptr) 
                  << ", has_piece=" << hasPiece(index) << ")" << std::endl;
        return false;
    }

    const int BLOCK_SIZE = 16 * 1024;
    const int MAX_PENDING_REQUESTS = 5;  // Pipeline 5 requests at once
    data.clear();
    data.resize(length);
    
    try {
        int remaining_length = length;
        int offset = 0;
        std::queue<int> pending_offsets;

        while (remaining_length > 0 || !pending_offsets.empty()) {
            // Send requests until pipeline is full
            while (remaining_length > 0 && pending_offsets.size() < MAX_PENDING_REQUESTS) {
                int block_length = std::min(BLOCK_SIZE, remaining_length);
                
                // Prepare and send request
                std::vector<uint8_t> request_payload(12);
                PeerUtils::addIntToPayload(request_payload, index, 0);
                PeerUtils::addIntToPayload(request_payload, offset, 4);
                PeerUtils::addIntToPayload(request_payload, block_length, 8);
                peer_utils->sendMessage(PeerMessageType::REQUEST, request_payload);
                
                pending_offsets.push(offset);
                offset += block_length;
                remaining_length -= block_length;
            }

            // Receive one block
            unsigned char msg_length_buf[4];
            char msg_type;
            std::vector<uint8_t> payload;
            peer_utils->receiveMessage(msg_length_buf, msg_type, payload);

            if (msg_type != static_cast<char>(PeerMessageType::PIECE)) {
                std::cerr << "Unexpected message type: " << static_cast<int>(msg_type) << std::endl;
                return false;
            }

            // Validate response
            if (payload.size() < 8) {  // At least need index and begin fields
                std::cerr << "Invalid payload size" << std::endl;
                return false;
            }

            int recv_index = (payload[0] << 24) | (payload[1] << 16) |
                           (payload[2] << 8) | payload[3];
            int recv_begin = (payload[4] << 24) | (payload[5] << 16) |
                           (payload[6] << 8) | payload[7];

            if (recv_index != index || recv_begin != pending_offsets.front()) {
                return false;
            }

            // Copy block data to correct position
            std::copy(payload.begin() + 8, payload.end(), 
                     data.begin() + pending_offsets.front());
            pending_offsets.pop();
        }

        return data.size() == length;

    } catch (const std::exception& e) {
        std::cerr << "Peer " << getPeerInfo() << " download piece " 
                  << index << " failed: " << e.what() << std::endl;
        return false;
    }
}

bool PeerManager::hasPiece(int index) const {
    if (index < 0 || index >= piece_availability.size()) {
        return false;
    }
    return piece_availability[index];
}

void PeerManager::disconnect() {
    if (peer_utils) {
        peer_utils.reset();
    }
}

void PeerManager::processBitfield(const std::vector<uint8_t>& bitfield) {
    piece_availability.clear();
    piece_availability.reserve(bitfield.size() * 8);

    for (uint8_t byte : bitfield) {
        for (int i = 7; i >= 0; --i) {
            piece_availability.push_back((byte >> i) & 1);
        }
    }
}
