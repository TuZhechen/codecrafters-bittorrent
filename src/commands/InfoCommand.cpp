#include "../utils/SHA1.hpp"
#include "InfoCommand.hpp"
#include <iostream>
#include <sstream>
#include <fstream>

void InfoCommand::execute(const std::vector<std::string>& args) {
    try {
        std::string torrentContent = readTorrentFile(args[0]);
        
        BencodeDecoder decoder;
        std::string content = torrentContent;   
        nlohmann::json torrentData = decoder.decode(content);
        
        displayTorrentInfo(torrentData);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to process torrent file: " + std::string(e.what()));
    }
}

std::string InfoCommand::readTorrentFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open torrent file: " + filepath);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();

}

void InfoCommand::displayTorrentInfo(const nlohmann::json& torrentData) {
    if (!torrentData.contains("announce") || !torrentData["announce"].is_string()) {
        throw std::runtime_error("Invalid torrent file: missing or invalid announce URL");
    }
    std::cout << "Tracker URL: " << torrentData["announce"].get<std::string>() << std::endl;

    if (!torrentData.contains("info") || !torrentData["info"].is_object()) {
        throw std::runtime_error("Invalid torrent file: missing or invalid info dictionary");
    }

    const auto& info = torrentData["info"];
    if (!info.contains("length") || !info["length"].is_number()) {
        throw std::runtime_error("Invalid torrent info: missing or invalid file length");
    }
    std::cout << "Length: " << info["length"].get<int64_t>() << std::endl;

    // Calculate and display info hash
    BencodeEncoder encoder;
    std::string encoded_info = encoder.encode(info);
    
    auto hash = SHA1::calculate(encoded_info);
    std::cout << "Info Hash: " << SHA1::toHex(hash) << std::endl;

    if (!info.contains("piece length") || !info["piece length"].is_number()) {
        throw std::runtime_error("Invalid torrent info: missing or invalid piece length");
    }
    std::cout << "Piece Length: " << info["piece length"].get<int64_t>() << std::endl;

    if (!info.contains("pieces") || !info["pieces"].is_string()) {
        throw std::runtime_error("Invalid torrent info: missing or invalid pieces SHA1 hashes");
    }
    std::string pieces = info["pieces"].get<std::string>();
    
    std::cout << "Piece Hashes:" << std::endl;
    for (size_t i = 0; i < pieces.length(); i += 20) {
        std::array<unsigned char, 20> piece_hash;
        std::copy(pieces.begin() + i, pieces.begin() + i + 20, piece_hash.begin());
        std::cout << SHA1::toHex(piece_hash) << std::endl;
    }
        
}
