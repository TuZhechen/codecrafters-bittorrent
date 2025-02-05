#pragma once
#include "BencodeDecoder.hpp"
#include "BencodeEncoder.hpp"

class Bencode {
public:
    static nlohmann::json decode(std::string& encoded_value) {
        return BencodeDecoder().decode(encoded_value);
    }
    
    static std::string encode(const nlohmann::json& value) {
        return BencodeEncoder().encode(value);
    }
}; 