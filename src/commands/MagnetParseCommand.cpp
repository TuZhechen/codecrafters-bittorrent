#include "MagnetParseCommand.hpp"
#include "../utils/MagnetUtils.hpp"
#include <iostream>

void MagnetParseCommand::execute(const CommandOptions& options) {
    try {
        if (options.args.empty()) {
            throw std::runtime_error("No magnet link provided");
        }
        std::string magnetLink = options.args[0];
        displayMagnetInfo(magnetLink);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to process magnet link: " + std::string(e.what()));
    }
}

void MagnetParseCommand::displayMagnetInfo(const std::string& magnetLink) {
    if (magnetLink.empty()) {
        throw std::runtime_error("No magnet link provided");
    }
    if (magnetLink.find("magnet:") == std::string::npos) {
        throw std::runtime_error("Invalid magnet link: wrong head");
    }
    if (magnetLink.find("xt=") == std::string::npos) {
        throw std::runtime_error("Invalid magnet link: missing xt");
    }
    int hashStart = magnetLink.find("urn:btih:");
    if (hashStart == std::string::npos) {
        throw std::runtime_error("Invalid magnet link: missing urn:btih:");
    }
    int trackerStart = magnetLink.find("&tr=");
    if (trackerStart != std::string::npos) {
        std::string trackerUrl = magnetLink.substr(trackerStart + 4);
        std::cout << "Tracker URL: " << MagnetUtils::urlDecode(trackerUrl) << std::endl;
    }
    std::string infoHash = magnetLink.substr(hashStart + 9, 40);
    std::cout << "Info Hash: " << infoHash << std::endl;
}
