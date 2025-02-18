#include "TorrentUtils.hpp"
#include <curl/curl.h>
#include <sstream>
#include <iomanip>
#include <random>
#include <sys/socket.h>
#include <stdexcept>
#include <fstream>
#include <iostream>

size_t TorrentUtils::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string TorrentUtils::makeTrackerRequest(const std::string& announce_url, 
                                           const std::string& info_hash,
                                           int64_t length) {
    CURL* curl = curl_easy_init();
    std::string response;
    
    if (curl) {
        std::string peer_id = "-CC0001-123456789012";
        std::stringstream ss;
        ss << announce_url
           << "?info_hash=" << urlEncode(reinterpret_cast<const unsigned char*>(info_hash.c_str()), 20)
           << "&peer_id=" << urlEncode(reinterpret_cast<const unsigned char*>(peer_id.c_str()), 20)
           << "&port=6881"
           << "&uploaded=0"
           << "&downloaded=0"
           << "&left=" << length
           << "&compact=1";
        
        std::string url = ss.str();
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            throw std::runtime_error("Failed to contact tracker");
        }
    }
    
    return response;
}

std::string TorrentUtils::urlEncode(const unsigned char* data, size_t len) {
    std::stringstream ss;
    for (size_t i = 0; i < len; i++) {
        if (isalnum(data[i]) || data[i] == '-' || data[i] == '_' || data[i] == '.' || data[i] == '~') {
            ss << data[i];
        } else {
            ss << "%" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') 
               << static_cast<int>(data[i]);
        }
    }
    return ss.str();
}

void TorrentUtils::performHandshake(int sock, const std::string& info_hash) {
    // Send handshake
    std::string protocol = "BitTorrent protocol";
    std::vector<uint8_t> handshake;
    handshake.reserve(68);  // Total handshake length
    
    // Protocol length and protocol string
    handshake.push_back(19);
    handshake.insert(handshake.end(), protocol.begin(), protocol.end());
    
    // Reserved bytes
    handshake.insert(handshake.end(), 8, 0);
    
    // Info hash
    handshake.insert(handshake.end(), info_hash.begin(), info_hash.end());
    
    // Peer ID (random 20 bytes)
    std::string peer_id = "1a0e74aeaff2e16395e2";
    handshake.insert(handshake.end(), peer_id.begin(), peer_id.end());
    
    if (send(sock, handshake.data(), handshake.size(), 0) != handshake.size()) {
        throw std::runtime_error("Failed to send handshake");
    }
    
    // Receive handshake response
    std::vector<uint8_t> response(68);
    if (recv(sock, response.data(), response.size(), 0) != response.size()) {
        throw std::runtime_error("Failed to receive handshake");
    }
    
    // Extract and format peer ID from response
    std::string received_peer_id(response.begin() + 48, response.end());
    
    // Convert peer ID to hex string
    std::stringstream ss;
    for (unsigned char c : received_peer_id) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    
    std::cout << "Peer ID: " << ss.str() << std::endl;
}

std::string TorrentUtils::readTorrentFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open torrent file: " + filepath);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
} 