#include "PeersCommand.hpp"
#include <iostream>
#include <curl/curl.h>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <stdexcept>

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void PeersCommand::execute(const std::vector<std::string>& args) {
    try {
        std::string torrentContent = readTorrentFile(args[0]);
        
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
        
        // Make tracker request
        std::string response = makeTrackerRequest(announce_url, 
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

std::string PeersCommand::makeTrackerRequest(const std::string& announce_url, 
                                           const std::string& info_hash,
                                           int64_t length) {
    CURL* curl = curl_easy_init();
    std::string response;
    
    if (curl) {
        std::string peer_id = "-CC0001-123456789012"; // 20 bytes
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
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            throw std::runtime_error("Failed to contact tracker");
        }
    }
    
    return response;
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

std::string PeersCommand::urlEncode(const unsigned char* data, size_t len) {
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

std::string PeersCommand::readTorrentFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open torrent file: " + filepath);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
} 