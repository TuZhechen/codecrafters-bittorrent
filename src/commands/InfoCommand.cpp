#include "InfoCommand.hpp"
#include <iostream>
#include <sstream>
#include <fstream>

void InfoCommand::execute(const std::string& input) {
    try {
        std::string torrentContent = readTorrentFile(input);
        
        BencodeParser parser;
        std::string content = torrentContent;
        nlohmann::json torrentData = parser.decode(content);
        

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
}
