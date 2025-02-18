#include "DownloadPieceCommand.hpp"
#include "../protocol/PeerMessage.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <vector>
#include <algorithm>

void DownloadPieceCommand::execute(const CommandOptions& options) {
    try {
        if (options.args.size() != 2) {
            throw std::runtime_error("Expected: <torrent_file> <piece_index>");
        }
        
        std::string torrent_file = options.args[0];
        int piece_index = std::stoi(options.args[1]);
        std::string output_file = options.options.at("-o");
        
        // Parse torrent file
        std::string torrent_content = TorrentUtils::readTorrentFile(torrent_file);
        BencodeDecoder decoder;
        nlohmann::json torrent_data = decoder.decode(torrent_content);
        
        // Get piece info
        const auto& info = torrent_data["info"];
        int piece_length = info["piece length"];
        int64_t file_length = info["length"].get<int64_t>();
        int total_pieces = (file_length + piece_length - 1) / piece_length;
        
        // Calculate actual piece length (important for last piece)
        int actual_piece_length = (piece_index == total_pieces - 1)
            ? (file_length % piece_length) : piece_length;
        if (actual_piece_length == 0) {
            actual_piece_length = piece_length;  // Handle case when file_length is multiple of piece_length
        }

        std::string piece_hash = info["pieces"].get<std::string>().substr(piece_index * 20, 20);
        
        // Calculate info hash for tracker request
        BencodeEncoder encoder;
        std::string encoded_info = encoder.encode(info);
        auto hash = SHA1::calculate(encoded_info);
        std::string info_hash(reinterpret_cast<char*>(hash.data()), 20);
        
        // Get peers from tracker
        std::string announce_url = torrent_data["announce"];
        std::string tracker_response = TorrentUtils::makeTrackerRequest(
            announce_url, info_hash, info["length"]);
        
        nlohmann::json resp_data = decoder.decode(tracker_response);
        std::string peers_data = resp_data["peers"].get<std::string>();
        
        std::cout << "Found " << peers_data.length() / 6 << " peers" << std::endl;
        
        // Try connecting to peers
        for (size_t i = 0; i < peers_data.length(); i += 6) {
            try {
                // Get peer IP and port
                unsigned char ip[4];
                memcpy(ip, peers_data.c_str() + i, 4);
                uint16_t port = static_cast<unsigned char>(peers_data[i + 4]) << 8 | 
                               static_cast<unsigned char>(peers_data[i + 5]);
                
                std::string ip_str = std::to_string(ip[0]) + "." + 
                                   std::to_string(ip[1]) + "." + 
                                   std::to_string(ip[2]) + "." + 
                                   std::to_string(ip[3]);
                
                std::cout << "Trying peer " << ip_str << ":" << port << std::endl;
                
                // Create socket
                sock = socket(AF_INET, SOCK_STREAM, 0);
                if (sock < 0) {
                    std::cout << "Failed to create socket for " << ip_str << std::endl;
                    continue;
                }

                // Connect to peer
                struct sockaddr_in peer_addr;
                peer_addr.sin_family = AF_INET;
                peer_addr.sin_port = htons(port);
                memcpy(&peer_addr.sin_addr, ip, 4);
                
                if (connect(sock, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0) {
                    std::cout << "Failed to connect to " << ip_str << ":" << port << std::endl;
                    close(sock);
                    continue;
                }

                std::cout << "Connected to " << ip_str << ":" << port << std::endl;
                
                // Perform handshake
                std::cout << "Performing handshake..." << std::endl;
                TorrentUtils::performHandshake(sock, info_hash);
                std::cout << "Handshake successful" << std::endl;

                // Message exchange
                char msg_length_buf[4];
                char msg_type;
                std::vector<uint8_t> payload_recv, payload_send;
                
                // 1. Receive bitfield
                std::cout << "Waiting for bitfield..." << std::endl;
                receiveMessage(msg_length_buf, msg_type, payload_recv);
                if (msg_type != static_cast<char>(PeerMessageType::BITFIELD)) {
                    throw std::runtime_error("Expected bitfield message from peer");
                }
                std::cout << "Received bitfield" << std::endl;
                payload_recv.clear();
                
                // 2. Send interested
                std::cout << "Sending interested message..." << std::endl;
                sendMessage(PeerMessageType::INTERESTED, {});
                std::cout << "Interested message sent" << std::endl;
                
                // 3. Receive unchoke
                std::cout << "Waiting for unchoke..." << std::endl;
                receiveMessage(msg_length_buf, msg_type, payload_recv);
                if (msg_type != static_cast<char>(PeerMessageType::UNCHOKE)) {
                    throw std::runtime_error("Expected unchoke message from peer");
                }
                std::cout << "Received unchoke" << std::endl;
                payload_recv.clear();
                
                std::cout << "Downloading piece " << piece_index << " (" << actual_piece_length << " bytes)" << std::endl;
                
                // Download piece in blocks
                std::vector<uint8_t> complete_piece;
                complete_piece.reserve(actual_piece_length);
                
                int block_size = 16 * 1024;
                int block_offset = 0;
                int remaining_length = actual_piece_length;
                
                while (remaining_length > 0) {
                    int current_block_length = std::min(block_size, remaining_length);
                    
                    // Send request for block
                    payload_send.clear();
                    payload_send.resize(12);
                    addIntToPayload(payload_send, piece_index, 0);
                    addIntToPayload(payload_send, block_offset, 4);
                    addIntToPayload(payload_send, current_block_length, 8);
                    sendMessage(PeerMessageType::REQUEST, payload_send);
                    
                    // Receive piece message
                    receiveMessage(msg_length_buf, msg_type, payload_recv);
                    if (msg_type != static_cast<char>(PeerMessageType::PIECE)) {
                        throw std::runtime_error("Expected piece message from peer");
                    }
                    
                    // Validate block metadata
                    int recv_index = (payload_recv[0] << 24) | (payload_recv[1] << 16) |
                                   (payload_recv[2] << 8) | payload_recv[3];
                    int recv_begin = (payload_recv[4] << 24) | (payload_recv[5] << 16) |
                                   (payload_recv[6] << 8) | payload_recv[7];
                    
                    if (recv_index != piece_index || recv_begin != block_offset) {
                        throw std::runtime_error("Received incorrect piece/offset");
                    }
                    
                    // Add block to piece
                    complete_piece.insert(complete_piece.end(), 
                                        payload_recv.begin() + 8,
                                        payload_recv.end());
                    
                    payload_recv.clear();
                    block_offset += block_size;
                    remaining_length -= current_block_length;
                }
                
                // Validate piece hash
                auto piece_hash_calc = SHA1::calculate(
                    std::string(complete_piece.begin(), complete_piece.end())
                );
                if (piece_hash != std::string(reinterpret_cast<char*>(piece_hash_calc.data()), 20)) {
                    throw std::runtime_error("Piece hash verification failed");
                }
                
                // Write to output file
                std::ofstream output(output_file, std::ios::binary);
                if (!output) {
                    throw std::runtime_error("Failed to open output file: " + output_file);
                }
                output.write(reinterpret_cast<char*>(complete_piece.data()), complete_piece.size());
                
                // Successfully downloaded piece
                return;
                
            } catch (const std::exception& e) {
                if (sock >= 0) {
                    close(sock);
                }
                throw std::runtime_error("Download piece failed: " + std::string(e.what()));
            }
        }
        
        throw std::runtime_error("No valid peers found");
        
    } catch (const std::exception& e) {
        if (sock >= 0) {
            close(sock);
        }
        throw std::runtime_error("Download piece failed: " + std::string(e.what()));
    }
}

void DownloadPieceCommand::receiveMessage(char* msg_length_buf, char& msg_type, std::vector<uint8_t>& payload) {
    if (recv(sock, msg_length_buf, 4, 0) != 4) {
        throw std::runtime_error("Failed to receive message");
    }

    // Cast to unsigned char before shifting
    uint32_t msg_length = ((unsigned char)msg_length_buf[0] << 24) | 
                         ((unsigned char)msg_length_buf[1] << 16) |
                         ((unsigned char)msg_length_buf[2] << 8) | 
                         (unsigned char)msg_length_buf[3];

    // Add sanity check for message length
    if (msg_length > 16384 + 9) {  // Max block size (16KB) + metadata
        throw std::runtime_error("Message length too large: " + std::to_string(msg_length));
    }

    std::cout << "Receiving message type..." << std::endl;
    if (recv(sock, &msg_type, 1, 0) != 1) {
        throw std::runtime_error("Failed to receive message type");
    }
    std::cout << "Message type: " << static_cast<int>(msg_type) << std::endl;

    std::cout << "Receiving payload of size " << (msg_length - 1) << "..." << std::endl;
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
            std::cout << "Received " << received << " bytes, total " 
                      << total_received << " of " << payload.size() << std::endl;
        }
    }
    std::cout << "Payload received" << std::endl;
}

void DownloadPieceCommand::sendMessage(PeerMessageType msg_type, const std::vector<uint8_t>& payload) {
    std::cout << "Creating message type " << static_cast<int>(msg_type) 
              << " with payload size " << payload.size() << std::endl;
    
    auto msg = PeerMessage::create(msg_type, payload);
    auto serialized = msg.serialize();
    
    uint32_t length = msg.getLength();
    char length_buf[4] = {
        static_cast<char>((length >> 24) & 0xFF),
        static_cast<char>((length >> 16) & 0xFF),
        static_cast<char>((length >> 8) & 0xFF),
        static_cast<char>(length & 0xFF)
    };
    
    std::cout << "Sending message length " << length << "..." << std::endl;
    if (send(sock, length_buf, 4, 0) != 4) {
        throw std::runtime_error("Failed to send message length");
    }
    
    std::cout << "Sending message body..." << std::endl;
    if (send(sock, serialized.data(), serialized.size(), 0) != serialized.size()) {
        throw std::runtime_error("Failed to send message body");
    }
    std::cout << "Message sent successfully" << std::endl;
}

void DownloadPieceCommand::addIntToPayload(std::vector<uint8_t>& payload, int value, int offset) {
    payload[offset] = (value >> 24) & 0xFF;
    payload[offset + 1] = (value >> 16) & 0xFF;
    payload[offset + 2] = (value >> 8) & 0xFF;
    payload[offset + 3] = value & 0xFF;
}
