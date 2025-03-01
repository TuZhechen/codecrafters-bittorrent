#pragma once
#include <string>
#include "../bencode/Bencode.hpp"
#include <vector>

// Add this struct to hold handshake results
struct HandshakeResult {
    int extension_id;
    std::vector<uint8_t> bitfield;
};

class MagnetUtils {
public:
    static std::string urlDecode(const std::string& encoded);
    static std::string urlEncode(const unsigned char* data, size_t len);
    static std::string makeTrackerRequest(const std::string& announce_url, 
                                           const std::string& info_hash,
                                           int64_t length = 999);
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static nlohmann::json parseMagnetLink(const std::string& magnet_link);
    static HandshakeResult performHandshake(int sock, const std::string& info_hash, bool silent = false);
    static void requestMetadata(int sock, int extension_id);
    static nlohmann::json receiveMetadata(int sock, const std::string& info_hash);
}; 