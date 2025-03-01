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

int MagnetUtils::performHandshake(int sock, const std::string& info_hash, bool silent) {
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
    
    if (!silent) {
        std::cout << "Peer ID: " << ss.str() << std::endl;
    }

    // TBD: send the bitfield message

    
    // Receive the bitfield message
    uint8_t bitfield_length_buf[4];
    if (recv(sock, bitfield_length_buf, 4, 0) != 4) {
        throw std::runtime_error("Failed to receive bitfield length");
    }
    uint32_t bitfield_length = (bitfield_length_buf[0] << 24) | (bitfield_length_buf[1] << 16) | 
                             (bitfield_length_buf[2] << 8) | bitfield_length_buf[3];

    std::vector<uint8_t> bitfield(bitfield_length);
    if (recv(sock, bitfield.data(), bitfield_length, 0) != bitfield_length) {
        throw std::runtime_error("Failed to receive bitfield");
    }

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

        // Receive the extension handshake response
        // First receive the message length (4 bytes)
        uint8_t length_buf[4];
        if (recv(sock, length_buf, 4, 0) != 4) {
            throw std::runtime_error("Failed to receive extension handshake length");
        }

        uint32_t received_message_length = (length_buf[0] << 24) | (length_buf[1] << 16) | 
                                 (length_buf[2] << 8) | length_buf[3];

        // Receive message type (1 byte)
        uint8_t message_type;
        if (recv(sock, &message_type, 1, 0) != 1) {
            throw std::runtime_error("Failed to receive extension message type");
        }
        // Receive extension message ID (1 byte)
        uint8_t ext_message_id;
        if (recv(sock, &ext_message_id, 1, 0) != 1) {
            throw std::runtime_error("Failed to receive extension message ID");
        }
        // Receive payload (message_length - 2 bytes for the IDs)
        std::vector<uint8_t> received_payload_bytes(received_message_length - 2);
        if (recv(sock, received_payload_bytes.data(), received_payload_bytes.size(), 0) != received_payload_bytes.size()) {
            throw std::runtime_error("Failed to receive extension handshake payload");
        }

        // Convert payload to string and decode
        std::string received_payload_str(received_payload_bytes.begin(), received_payload_bytes.end());
        nlohmann::json received_payload = Bencode::decode(received_payload_str);
        if (received_payload.contains("m") && received_payload["m"].contains("ut_metadata")) {
            int extension_id = received_payload["m"]["ut_metadata"].get<int>();
            if (!silent) {
                std::cout << "Peer Metadata Extension ID: " << extension_id << std::endl;
            }
            return extension_id;
        }
    }
    return -1;
}

void MagnetUtils::requestMetadata(int sock, int extension_id) {
    // Create the metadata request payload
    nlohmann::json payload;
    payload["msg_type"] = 0;
    payload["piece"] = 0;

    // Bencode the payload
    std::string payload_str = Bencode::encode(payload);
        
    // Calculate the message length (payload size + 2 bytes for IDs)
    uint32_t message_length = payload_str.size() + 2;
        
    // Construct the complete extension handshake message
    std::vector<uint8_t> metadata_request;
    metadata_request.reserve(6 + payload_str.size());
        
    // Add length prefix (4 bytes, big-endian)
    metadata_request.push_back((message_length >> 24) & 0xFF);
    metadata_request.push_back((message_length >> 16) & 0xFF);
    metadata_request.push_back((message_length >> 8) & 0xFF);
    metadata_request.push_back(message_length & 0xFF);
        
    // Add message ID (1 byte)
    metadata_request.push_back(20);  // 20 for extension protocol

    // Add extension ID (1 byte)
    metadata_request.push_back(static_cast<uint8_t>(extension_id));
        
    // Add bencoded payload
    metadata_request.insert(metadata_request.end(), 
                                  payload_str.begin(), payload_str.end());
        
    // Send the metadata request
    if (send(sock, metadata_request.data(), metadata_request.size(), 0) != 
            static_cast<ssize_t>(metadata_request.size())) {
        throw std::runtime_error("Failed to send metadata request");
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