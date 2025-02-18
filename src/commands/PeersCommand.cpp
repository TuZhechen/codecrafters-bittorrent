#include "PeersCommand.hpp"
#include <iostream>
#include <curl/curl.h>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <stdexcept>

void PeersCommand::execute(const CommandOptions& options) {
    try {
        if (options.args.empty()) {
            throw std::runtime_error("No torrent file provided");
        }
        std::string torrentContent = TorrentUtils::readTorrentFile(options.args[0]);
        
        BencodeDecoder decoder;
        std::string content = torrentContent;
        nlohmann::json torrentData = decoder.decode(content);
        
        const auto& info = torrentData["info"];
        BencodeEncoder encoder;
        std::string encoded_info = encoder.encode(info);
        
        // Calculate info hash
        auto hash = SHA1::calculate(encoded_info);
        
        // Get tracker URL and file length
        std::string announce_url = torrentData["announce"].get<std::string>();
        int64_t length = info["length"].get<int64_t>();
        
        std::string response = TorrentUtils::makeTrackerRequest(announce_url, 
            std::string(reinterpret_cast<char*>(hash.data()), 20),
            length);
        
        // Parse response
        std::string resp_content = response;
        nlohmann::json resp_data = decoder.decode(resp_content);
        
        // Display peers
        displayPeers(resp_data["peers"].get<std::string>());
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to process peers: " + std::string(e.what()));
    }
}

void PeersCommand::displayPeers(const std::string& peers_data) {
    for (size_t i = 0; i < peers_data.length(); i += 6) {
        // First 4 bytes are IP
        unsigned char ip[4];
        memcpy(ip, peers_data.c_str() + i, 4);
        
        // Last 2 bytes are port
        uint16_t port = static_cast<unsigned char>(peers_data[i + 4]) << 8 | 
                       static_cast<unsigned char>(peers_data[i + 5]);
        
        std::cout << static_cast<int>(ip[0]) << "."
                 << static_cast<int>(ip[1]) << "."
                 << static_cast<int>(ip[2]) << "."
                 << static_cast<int>(ip[3]) << ":"
                 << port << std::endl;
    }
}