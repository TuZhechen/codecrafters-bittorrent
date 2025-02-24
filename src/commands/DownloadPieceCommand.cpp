#include "DownloadPieceCommand.hpp"
#include "../protocol/PeerMessage.hpp"
#include "../manager/PeerManager.hpp"
#include "../manager/PieceManager.hpp"
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
        std::string pieces_hash = info["pieces"].get<std::string>();
        int total_pieces = (file_length + piece_length - 1) / piece_length;

        // Validate piece index
        if (piece_index < 0 || piece_index >= total_pieces) {
            throw std::runtime_error("Invalid piece index");
        }

        // Calculate info hash
        BencodeEncoder encoder;
        std::string encoded_info = encoder.encode(info);
        auto hash = SHA1::calculate(encoded_info);
        std::string info_hash(reinterpret_cast<char*>(hash.data()), 20);

        // Initialize piece manager (only for this piece)
        auto piece_manager = std::make_unique<PieceManager>(
            total_pieces, piece_length, file_length, info_hash, pieces_hash
        );

        // Connect to peers
        std::string tracker_response = TorrentUtils::makeTrackerRequest(
            torrent_data["announce"].get<std::string>(), 
            info_hash, 
            file_length
        );

        nlohmann::json resp_data = decoder.decode(tracker_response);
        std::string peers_data = resp_data["peers"].get<std::string>();
        
        std::cout << "Found " << peers_data.length() / 6 << " peers" << std::endl;
        
        // Try each peer until successful
        for (size_t i = 0; i < peers_data.length(); i += 6) {
            unsigned char ip_bytes[4];
            memcpy(ip_bytes, peers_data.c_str() + i, 4);
            uint16_t port = static_cast<unsigned char>(peers_data[i + 4]) << 8 | 
                           static_cast<unsigned char>(peers_data[i + 5]);

            std::string ip_str = std::to_string(ip_bytes[0]) + "." + 
                                std::to_string(ip_bytes[1]) + "." + 
                                std::to_string(ip_bytes[2]) + "." + 
                                std::to_string(ip_bytes[3]);
                
            auto peer = std::make_unique<PeerManager>(ip_str, port, info_hash);
            if (!peer->connect()) {
                continue;
            }

            if (!peer->hasPiece(piece_index)) {
                continue;
            }

            std::vector<uint8_t> piece_data;
            int piece_length = piece_manager->getPieceLength(piece_index);

            std::cout << "Downloading piece " << piece_index 
                      << " from peer " << peer->getPeerInfo() << std::endl;

            if (peer->downloadPiece(piece_index, piece_length, piece_data)) {
                if (piece_manager->verifyPiece(piece_index, piece_data)) {
                    // Write verified piece to file
                    std::ofstream output(output_file, std::ios::binary);
                    if (!output.write(reinterpret_cast<char*>(piece_data.data()), 
                                    piece_data.size())) {
                        throw std::runtime_error("Failed to write output file");
                    }
                    
                    std::cout << "Piece " << piece_index << " downloaded successfully. "
                              << "Length: " << piece_data.size() << std::endl;
                    return;
                }
            }
        }
        
        throw std::runtime_error("Failed to download piece from any peer");
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Download piece failed: " + std::string(e.what()));
    }
}