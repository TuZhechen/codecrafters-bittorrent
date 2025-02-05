#include "BencodeEncoder.hpp"
#include <map>
#include <stdexcept>

std::string BencodeEncoder::encode(const nlohmann::json& value) {
    if (value.is_string()) {
        return encode_string(value.get<std::string>());
    } else if (value.is_number()) {
        return encode_integer(value.get<int64_t>());
    } else if (value.is_array()) {
        return encode_list(value);
    } else if (value.is_object()) {
        return encode_dictionary(value);
    } else {
        throw std::runtime_error("Unsupported JSON type for bencode encoding");
    }
}

std::string BencodeEncoder::encode_string(const std::string& str) {
    return std::to_string(str.length()) + ":" + str;
}

std::string BencodeEncoder::encode_integer(int64_t value) {
    return "i" + std::to_string(value) + "e";
}

std::string BencodeEncoder::encode_list(const nlohmann::json& list) {
    std::string result = "l";
    for (const auto& item : list) {
        result += encode(item);
    }
    result += "e";
    return result;
}

std::string BencodeEncoder::encode_dictionary(const nlohmann::json& dict) {
    std::string result = "d";
    // Sort keys to ensure consistent encoding
    std::map<std::string, nlohmann::json> sorted_dict;
    for (auto it = dict.begin(); it != dict.end(); ++it) {
        sorted_dict[it.key()] = it.value();
    }
    
    for (const auto& [key, value] : sorted_dict) {
        result += encode(key) + encode(value);
    }
    result += "e";
    return result;
} 