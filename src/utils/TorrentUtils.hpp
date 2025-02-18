#pragma once
#include <string>
#include <cstdint>

class TorrentUtils {
public:
    static std::string makeTrackerRequest(const std::string& announce_url, 
                                        const std::string& info_hash,
                                        int64_t length);
    static void performHandshake(int sock, const std::string& info_hash);
    static std::string urlEncode(const unsigned char* data, size_t len);
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static std::string readTorrentFile(const std::string& filepath);
}; 