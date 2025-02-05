#pragma once

#include <string>
#include <nlohmann/json.hpp>

class BencodeParser {
public:
    nlohmann::json decode(std::string& encoded_value);

private:
    nlohmann::json parse_string(std::string& encoded_value);
    nlohmann::json parse_integer(std::string& encoded_value);
    nlohmann::json parse_list(std::string& encoded_value);
    nlohmann::json parse_dictionary(std::string& encoded_value);
}; 