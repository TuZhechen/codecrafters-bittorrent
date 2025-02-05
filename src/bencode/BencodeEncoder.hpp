#pragma once
#include <string>
#include <nlohmann/json.hpp>

class BencodeEncoder {
public:
    std::string encode(const nlohmann::json& value);

private:
    std::string encode_string(const std::string& str);
    std::string encode_integer(int64_t value);
    std::string encode_list(const nlohmann::json& list);
    std::string encode_dictionary(const nlohmann::json& dict);
}; 