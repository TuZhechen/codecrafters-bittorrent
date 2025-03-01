#include "MagnetUtils.hpp"
#include <string>
#include <iomanip>
#include <cctype>
#include <sstream>
#include <curl/curl.h>
#include <vector>
#include <iostream>
#include <sys/socket.h>
#include "../protocol/PeerMessageType.hpp"
#include "../utils/SHA1.hpp"
#include <stdexcept>
#include <fstream>
#include <iostream>

size_t MagnetUtils::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

nlohmann::json MagnetUtils::parseMagnetLink(const std::string& magnet_link) {
    nlohmann::json result;
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
    result["info_hash"] = infoHash;

    std::string binaryInfoHash;
    for (size_t i = 0; i < 40; i += 2) {
        std::string hex_pair = infoHash.substr(i, 2);
        char byte = static_cast<char>(std::stoi(hex_pair, nullptr, 16));
        binaryInfoHash.push_back(byte);
    }
    result["binary_info_hash"] = binaryInfoHash;

    int trackerStart = magnet_link.find("&tr=");
    if (trackerStart == std::string::npos) {
        throw std::runtime_error("A tracker url is required");
    }
    std::string trackerUrl = MagnetUtils::urlDecode(magnet_link.substr(trackerStart + 4));
    std::cout << "Tracker URL: " << trackerUrl << std::endl;
    result["tracker_url"] = trackerUrl;
    return result;
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

HandshakeResult MagnetUtils::performHandshake(int sock, const std::string& info_hash, bool silent) {
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

    // Receive the bitfield message type
    uint8_t msg_type;
    if (recv(sock, &msg_type, 1, 0) != 1) {  
        throw std::runtime_error("Failed to receive bitfield message type");
    }
    if (msg_type != static_cast<uint8_t>(PeerMessageType::BITFIELD)) {  
        throw std::runtime_error("Expected bitfield message, got: " + std::to_string(msg_type));
    }

    std::vector<uint8_t> bitfield(bitfield_length - 1);
    if (recv(sock, bitfield.data(), bitfield.size(), 0) != bitfield.size()) {
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
            return HandshakeResult{extension_id, bitfield};
        }
    }
    return HandshakeResult{-1, bitfield};
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

nlohmann::json MagnetUtils::receiveMetadata(int sock, const std::string& info_hash) {
    // Receive message length (4 bytes)
    uint8_t length_buf[4];
    if (recv(sock, length_buf, 4, 0) != 4) {
        throw std::runtime_error("Failed to receive data message length");
    }

    uint32_t received_message_length = (length_buf[0] << 24) | (length_buf[1] << 16) | 
                                 (length_buf[2] << 8) | length_buf[3];

    // Receive message id (1 byte)
    uint8_t message_id;
    if (recv(sock, &message_id, 1, 0) != 1) {
        throw std::runtime_error("Failed to receive message id");
    }
    // Validate message id
    if (message_id != 20) {
        throw std::runtime_error("Received message ID is expected to be 20, but got " + std::to_string(message_id));
    }
    // Receive extension message ID (1 byte)
    uint8_t ext_message_id;
    if (recv(sock, &ext_message_id, 1, 0) != 1) {
        throw std::runtime_error("Failed to receive extension message ID");
    }

    // Receive payload (message_length - 2 bytes for the IDs)
    std::vector<uint8_t> received_payload_bytes(received_message_length - 2);
    if (recv(sock, received_payload_bytes.data(), received_payload_bytes.size(), 0) != received_payload_bytes.size()) {
        throw std::runtime_error("Failed to receive data message payload");
    }

    // Seperate metadata from payload
    size_t dict_end = 0;
    for (size_t i = 0; i < received_payload_bytes.size(); i++) {
        if (received_payload_bytes[i] == 'e' && received_payload_bytes[i+1] == 'e') {
            dict_end = i + 2;
            break;
        }
    }
    if (dict_end == 0) {
        throw std::runtime_error("Failed to find end of payload dictionary");
    }

    // Seperate and convert payload to string and decode
    std::string received_payload_str(received_payload_bytes.begin(), received_payload_bytes.begin() + dict_end);
    nlohmann::json received_payload = Bencode::decode(received_payload_str);
    // Validate the payload
    if (!received_payload.contains("msg_type") || 
        !received_payload.contains("piece") ||
        !received_payload.contains("total_size")) {
        throw std::runtime_error("Received payload does not contain msg_type, piece or total_size");
    }
    // Validate the message type
    if (received_payload["msg_type"].get<int>() != 1) {
        throw std::runtime_error("Received message type is expected to be 1, but got " + std::to_string(received_payload["msg_type"].get<int>()));
    }

    // Seperate and convert metadata to string, validate and decode
    std::string metadata_str(received_payload_bytes.begin() + dict_end, received_payload_bytes.end());
    auto hash = SHA1::calculate(metadata_str);

    // Convert calculated hash to hex
    std::string calculated_hash_hex;
    for (size_t i = 0; i < 20; i++) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02x", hash[i]);
        calculated_hash_hex += hex;
    }

    if (calculated_hash_hex != info_hash) {
        throw std::runtime_error("Metadata hash mismatch. Expected: " + info_hash + 
                               ", Got: " + calculated_hash_hex);
    }

    // Decode the metadata (info dictionary)
    nlohmann::json metadata = Bencode::decode(metadata_str);
    if (!metadata.contains("pieces") || 
        !metadata.contains("piece length") || 
        !metadata.contains("length")) {
        throw std::runtime_error("Received metadata does not contain pieces, piece length, or length");
    }

    std::cout << "Length: " << metadata["length"].get<int>() << std::endl;
    std::cout << "Info Hash: " << info_hash << std::endl;
    std::cout << "Piece Length: " << metadata["piece length"].get<int>() << std::endl;
    std::string pieces = metadata["pieces"].get<std::string>();
    
    std::cout << "Piece Hashes:" << std::endl;
    for (size_t i = 0; i < pieces.length(); i += 20) {
        std::array<unsigned char, 20> piece_hash;
        std::copy(pieces.begin() + i, pieces.begin() + i + 20, piece_hash.begin());
        std::cout << SHA1::toHex(piece_hash) << std::endl;
    }
    return metadata;
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