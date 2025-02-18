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

void HandshakeCommand::execute(const std::vector<std::string>& args) {
    try {
        if (args.size() != 2) {
            throw std::runtime_error("Invalid number of arguments. Expected: <torrent_file> <peer_address>");
        }

        std::string peer_addr = args[1];
        std::string torrent_file = args[0];
        std::string torrentContent = readTorrentFile(torrent_file);
        
        BencodeDecoder decoder;
        std::string content = torrentContent;
        nlohmann::json torrentData = decoder.decode(content);
        
        const auto& info = torrentData["info"];
        BencodeEncoder encoder;
        std::string encoded_info = encoder.encode(info);
        
        auto hash = SHA1::calculate(encoded_info);
        std::string info_hash(reinterpret_cast<char*>(hash.data()), 20);

        performHandshake(peer_addr, info_hash);
    } catch (const std::exception& e) {
        throw std::runtime_error("Handshake failed: " + std::string(e.what()));
    }
}

void HandshakeCommand::performHandshake(const std::string& peer_addr, const std::string& info_hash) {
    auto [ip, port] = parsePeerAddress(peer_addr);
    
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    // Connect to peer
    struct sockaddr_in peer_addr_struct;
    peer_addr_struct.sin_family = AF_INET;
    peer_addr_struct.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &peer_addr_struct.sin_addr) <= 0) {
        close(sock);
        throw std::runtime_error("Invalid peer address");
    }

    if (connect(sock, (struct sockaddr*)&peer_addr_struct, sizeof(peer_addr_struct)) < 0) {
        close(sock);
        throw std::runtime_error("Failed to connect to peer");
    }

    // Construct handshake message
    std::string handshake;
    handshake.push_back(19);  // protocol string length
    handshake += "BitTorrent protocol"; // protocol string
    handshake += std::string(8, '\0');  // reserved bytes
    handshake += info_hash;             // info hash

    // Generate random peer ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    std::string peer_id;
    for (int i = 0; i < 20; i++) {
        peer_id.push_back(static_cast<char>(dis(gen)));
    }
    handshake += peer_id;

    // Send handshake
    ssize_t sent_bytes = send(sock, handshake.c_str(), handshake.length(), 0);
    if (sent_bytes < 0 || static_cast<size_t>(sent_bytes) != handshake.length()) {
        close(sock);
        throw std::runtime_error("Failed to send handshake");
    }

    // Receive handshake response
    char response[68];
    ssize_t received_bytes = recv(sock, response, sizeof(response), 0);
    if (received_bytes < 0 || static_cast<size_t>(received_bytes) != sizeof(response)) {
        close(sock);
        throw std::runtime_error("Failed to receive handshake response");
    }

    close(sock);

    // Extract and print peer ID
    std::stringstream ss;
    ss << "Peer ID: ";
    for (int i = 48; i < 68; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(response[i]));
    }
    std::cout << ss.str() << std::endl;
}

std::pair<std::string, int> HandshakeCommand::parsePeerAddress(const std::string& peer_addr) {
    size_t colon_pos = peer_addr.find(':');
    if (colon_pos == std::string::npos) {
        throw std::runtime_error("Invalid peer address format. Expected: <ip>:<port>");
    }
    
    std::string ip = peer_addr.substr(0, colon_pos);
    int port = std::stoi(peer_addr.substr(colon_pos + 1));
    return {ip, port};
}

std::string HandshakeCommand::readTorrentFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open torrent file: " + filepath);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
} 