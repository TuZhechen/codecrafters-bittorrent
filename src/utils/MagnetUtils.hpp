#pragma once
#include <string>

class MagnetUtils {
public:
    static std::string urlDecode(const std::string& encoded);
    static std::string urlEncode(const unsigned char* data, size_t len);
    static std::string makeTrackerRequest(const std::string& announce_url, 
                                           const std::string& info_hash,
                                           int64_t length = 999);
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static void performHandshake(int sock, const std::string& info_hash);
}; 