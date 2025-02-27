#include "MagnetUtils.hpp"
#include <string>
#include <iomanip>
#include <cctype>
#include <sstream>
#include <curl/curl.h>
#include <vector>
#include <iostream>
#include <sys/socket.h>
#include <stdexcept>
#include <fstream>
#include <iostream>

size_t MagnetUtils::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string MagnetUtils::makeTrackerRequest(const std::string& announce_url, 
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

std::string MagnetUtils::urlEncode(const unsigned char* data, size_t len) {
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

void MagnetUtils::performHandshake(int sock, const std::string& info_hash) {
    // Prepare base handshake
    std::string protocol = "BitTorrent protocol";
    std::vector<uint8_t> handshake;
    handshake.reserve(68);  // Total handshake length
    
    // Protocol length and protocol string
    handshake.push_back(19);
    handshake.insert(handshake.end(), protocol.begin(), protocol.end());
    
    // Supported extensions
    handshake.insert(handshake.end(), 5, 0x00);
    handshake.insert(handshake.end(), 0x10);
    handshake.insert(handshake.end(), 2, 0x00);
    
    // Info hash
    handshake.insert(handshake.end(), info_hash.begin(), info_hash.end());
    
    // Peer ID (random 20 bytes)
    std::string peer_id = "1a0e74aeaff2e16395e2";
    handshake.insert(handshake.end(), peer_id.begin(), peer_id.end());
    
    if (send(sock, handshake.data(), handshake.size(), 0) != handshake.size()) {
        throw std::runtime_error("Failed to send handshake");
    }
    
    // Receive base handshake response
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

    // Check extension bits
    bool supports_extension = (response[25] & 0x10) != 0;
    if (supports_extension) {
        // Create the extension handshake payload
        nlohmann::json payload;
        payload["m"] = nlohmann::json::object();
        payload["m"]["ut_metadata"] = 1;
        payload["metadata_size"] = 0;
        
        // Bencode the payload
        std::string payload_str = Bencode::encode(payload);
        
        // Calculate the message length (payload size + 2 bytes for message IDs)
        uint32_t message_length = payload_str.size() + 2;
        
        // Construct the complete extension handshake message
        std::vector<uint8_t> extension_handshake;
        extension_handshake.reserve(6 + payload_str.size());
        
        // Add length prefix (4 bytes, big-endian)
        extension_handshake.push_back((message_length >> 24) & 0xFF);
        extension_handshake.push_back((message_length >> 16) & 0xFF);
        extension_handshake.push_back((message_length >> 8) & 0xFF);
        extension_handshake.push_back(message_length & 0xFF);
        
        // Add message ID (1 byte)
        extension_handshake.push_back(20);  // 20 for extension protocol
        
        // Add extension message ID (1 byte)
        extension_handshake.push_back(0);   // 0 for handshake
        
        // Add bencoded payload
        extension_handshake.insert(extension_handshake.end(), 
                                  payload_str.begin(), payload_str.end());
        
        // Send the extension handshake
        if (send(sock, extension_handshake.data(), extension_handshake.size(), 0) != 
                static_cast<ssize_t>(extension_handshake.size())) {
            throw std::runtime_error("Failed to send extension handshake");
        }
        
    }
}

std::string MagnetUtils::urlDecode(const std::string& encoded) {
    std::string decoded;
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%') {
            if (i + 2 < encoded.size()) {
                int value;
                std::istringstream iss(encoded.substr(i + 1, 2));
                if (iss >> std::hex >> value) {
                    decoded += static_cast<char>(value);
                    i += 2;
                }
            }
        } else {
            decoded += encoded[i];
        }
    }
    return decoded;
}