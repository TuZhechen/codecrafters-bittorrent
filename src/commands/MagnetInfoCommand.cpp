#include "MagnetInfoCommand.hpp"
#include "../utils/SHA1.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>

void MagnetInfoCommand::execute(const CommandOptions& options) {
    try {
        if (options.args.size() != 1) {
            throw std::runtime_error("Expected: <magnet_link>");
        }
        
        nlohmann::json magnet_data = MagnetUtils::parseMagnetLink(options.args[0]);
        std::string infoHash = magnet_data["info_hash"];
        std::string binaryInfoHash = magnet_data["binary_info_hash"];
        std::string trackerUrl = magnet_data["tracker_url"];

        std::string trackerResponse = MagnetUtils::makeTrackerRequest(trackerUrl, binaryInfoHash);
        nlohmann::json resp_data = Bencode::decode(trackerResponse);
        std::string peers_data = resp_data["peers"].get<std::string>();
        
        auto [ip, port] = PeerUtils::parsePeerAddress(peers_data, 0);
                
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        // Connect to peer
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
        int extension_id = MagnetUtils::performHandshake(sock, binaryInfoHash, true).extension_id;

        // Request metadata
        MagnetUtils::requestMetadata(sock, extension_id);

        // Receive metadata
        MagnetUtils::receiveMetadata(sock, infoHash);

    } catch (const std::exception& e) {
        if (sock >= 0) {
            close(sock);
        }
        throw std::runtime_error("Info request failed in " + std::string(__FUNCTION__) + 
                                 " at line " + std::to_string(__LINE__) + 
                                 ": " + std::string(e.what()));
    }
}