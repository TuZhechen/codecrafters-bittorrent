#pragma once
#include <string>
#include <array>

class SHA1 {
public:
    // Returns SHA1 hash as a 20-byte array
    static std::array<unsigned char, 20> calculate(const std::string& input);
    
    // Converts binary hash to hex string
    static std::string toHex(const std::array<unsigned char, 20>& hash);
}; 