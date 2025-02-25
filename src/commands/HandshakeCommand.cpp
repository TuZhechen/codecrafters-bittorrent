#include "HandshakeCommand.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <fstream>
#include <random>

void HandshakeCommand::execute(const CommandOptions& options) {
    try {
        if (options.args.size() != 2) {
            throw std::runtime_error("Expected: <torrent_file> <peer_address>");
        }
        
        std::string torrent_file = options.args[0];
        std::string peer_addr = options.args[1];
        
        std::string torrentContent = TorrentUtils::readTorrentFile(torrent_file);
        
        BencodeDecoder decoder;
        nlohmann::json torrentData = decoder.decode(torrentContent);
        
        const auto& info = torrentData["info"];
        BencodeEncoder encoder;
        std::string encoded_info = encoder.encode(info);
        
        auto hash = SHA1::calculate(encoded_info);
        std::string info_hash(reinterpret_cast<char*>(hash.data()), 20);

        // Parse peer address and create socket
        auto [ip, port] = PeerUtils::parsePeerAddress(peer_addr);
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
        TorrentUtils::performHandshake(sock, info_hash);

    } catch (const std::exception& e) {
        if (sock >= 0) {
            close(sock);
        }
        throw std::runtime_error("Handshake failed: " + std::string(e.what()));
    }
}