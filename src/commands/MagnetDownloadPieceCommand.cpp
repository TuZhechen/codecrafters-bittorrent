#include "MagnetDownloadPieceCommand.hpp"
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

void MagnetDownloadPieceCommand::execute(const CommandOptions& options) {
    try {
        if (options.args.size() != 2) {
            throw std::runtime_error("Expected: <magnet_link> <piece_index>");
        }
        if (!options.options.contains("-o")) {
            throw std::runtime_error("Expected: -o <output_file>");
        }
        
        std::string magnet_link = options.args[0];
        int piece_index = std::stoi(options.args[1]);
        std::string output_file = options.options.at("-o");
        
        // Parse magnet link
        nlohmann::json magnet_data = MagnetUtils::parseMagnetLink(magnet_link);
        std::string infoHash = magnet_data["info_hash"];
        std::string binaryInfoHash = magnet_data["binary_info_hash"];
        std::string trackerUrl = magnet_data["tracker_url"];

        // Connect to peers
        std::string tracker_response = MagnetUtils::makeTrackerRequest(
            trackerUrl, 
            binaryInfoHash
        );

        nlohmann::json resp_data = Bencode::decode(tracker_response);
        std::string peers_data = resp_data["peers"].get<std::string>();

        int sock = -1;

        // Handshake with each peer, request metadata and download piece
        for (size_t i = 0; i < peers_data.length(); i += 6) {
            auto [ip, port] = PeerUtils::parsePeerAddress(peers_data, i);
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                throw std::runtime_error("Failed to create socket");
            }

            // Connect to peer i
            struct sockaddr_in peer_addr_in;
            peer_addr_in.sin_family = AF_INET;
            peer_addr_in.sin_port = htons(port);
            if (inet_pton(AF_INET, ip.c_str(), &peer_addr_in.sin_addr) <= 0) {
                throw std::runtime_error("Invalid IP address");
            }

            if (connect(sock, (struct sockaddr*)&peer_addr_in, sizeof(peer_addr_in)) < 0) {
                throw std::runtime_error("Failed to connect to peer");
            }

            // Perform handshake
            auto [extension_id, bitfield] = MagnetUtils::performHandshake(sock, binaryInfoHash, true);

            // Request metadata
            MagnetUtils::requestMetadata(sock, extension_id);

            // Receive metadata
            nlohmann::json metadata = MagnetUtils::receiveMetadata(sock, infoHash);

            if(!metadata.is_null()) {
                int file_length = metadata["length"].get<int>();
                int piece_length = metadata["piece length"].get<int>();
                int total_pieces = (file_length + piece_length - 1) / piece_length;
                std::string pieces_hash = metadata["pieces"].get<std::string>();

                // Validate piece index
                if (piece_index < 0 || piece_index >= total_pieces) {
                    throw std::runtime_error("Invalid piece index");
                }

                // Initialize peer and piece manager
                auto peer = std::make_unique<PeerManager>(ip, port, infoHash);
                auto piece_manager = std::make_unique<PieceManager>(
                    total_pieces, piece_length, file_length, infoHash, pieces_hash
                );  

                // Download piece
                if(!peer->magnetConnect(sock, bitfield)) {
                    continue;
                }

                std::vector<uint8_t> piece_data;
                piece_length = piece_manager->getPieceLength(piece_index);

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
        }
        
        throw std::runtime_error("Failed to download piece from any peer");
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Download piece failed: " + std::string(e.what()));
    }
}