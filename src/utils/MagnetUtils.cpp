#include "MagnetUtils.hpp"
#include <string>
#include <cctype>
#include <sstream>

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