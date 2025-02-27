#include "MagnetHandshakeCommand.hpp"
#include "../utils/SHA1.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>

void MagnetHandshakeCommand::execute(const CommandOptions& options) {
    try {
        if (options.args.size() != 1) {
            throw std::runtime_error("Expected: <magnet_link>");
        }
        
        std::string magnet_link = options.args[0];
        if (magnet_link.empty()) {
            throw std::runtime_error("Magnet link cannot be empty");
        }
        if (magnet_link.find("magnet:") == std::string::npos) {
            throw std::runtime_error("Invalid magnet link: wrong header");
        }
        if (magnet_link.find("xt=") == std::string::npos) {
            throw std::runtime_error("Invalid magnet link: missing xt= parameter");
        }
        int hashStart = magnet_link.find("urn:btih:");
        if (hashStart == std::string::npos) {
            throw std::runtime_error("Invalid magnet link: missing urn:btih:");
        }
        std::string infoHash = magnet_link.substr(hashStart + 9, 40);
        std::string binaryInfoHash;
        for (size_t i = 0; i < 40; i += 2) {
            std::string hex_pair = infoHash.substr(i, 2);
            char byte = static_cast<char>(std::stoi(hex_pair, nullptr, 16));
            binaryInfoHash.push_back(byte);
        }

        int trackerStart = magnet_link.find("&tr=");
        if (trackerStart == std::string::npos) {
            throw std::runtime_error("A tracker url is required");
        }
        std::string trackerUrl = MagnetUtils::urlDecode(magnet_link.substr(trackerStart + 4));
        
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
        MagnetUtils::performHandshake(sock, binaryInfoHash);

    } catch (const std::exception& e) {
        if (sock >= 0) {
            close(sock);
        }
        throw std::runtime_error("Handshake failed: " + std::string(e.what()));
    }
}