#include "BencodeParser.hpp"
#include <vector>     
#include <map>        
#include <stdexcept>  
#include <cctype>    

nlohmann::json BencodeParser::decode(std::string& encoded_value) {
    if(std::isdigit(encoded_value[0])) {
        return parse_string(encoded_value);
    } else if(encoded_value[0] == 'i') {
        return parse_integer(encoded_value);
    } else if(encoded_value[0] == 'l') {
        return parse_list(encoded_value);
    } else if(encoded_value[0] == 'd') {
        return parse_dictionary(encoded_value);
    } else {
        throw std::runtime_error("Unhandled encoded value: " + encoded_value);
    }
}

nlohmann::json BencodeParser::parse_string(std::string& encoded_value) {
    size_t colon_index = encoded_value.find(':');
    if(colon_index == std::string::npos) {
        throw std::runtime_error("Invalid encoded string: missing colon" + encoded_value);
    }

    int64_t length = std::stoll(encoded_value.substr(0, colon_index));
    std::string content = encoded_value.substr(colon_index + 1, length);
    encoded_value = encoded_value.substr(colon_index + 1 + length);
    return nlohmann::json(content);
}

nlohmann::json BencodeParser::parse_integer(std::string& encoded_value) {
    size_t e_index = encoded_value.find('e');
    if(e_index == std::string::npos) {
        throw std::runtime_error("Invalid encoded integer: missing e" + encoded_value);
    }

    std::string integer_str = encoded_value.substr(1, e_index - 1);
    try {
        int64_t value = std::stoll(integer_str);
        encoded_value = encoded_value.substr(e_index + 1);
        return nlohmann::json(value);
    } catch(const std::exception& e) {
        throw std::runtime_error("Invalid encoded integer: unhandled format" + integer_str + " " + std::string(e.what()));
    }
}

nlohmann::json BencodeParser::parse_list(std::string& encoded_value) {
    std::vector<nlohmann::json> list;
    encoded_value = encoded_value.substr(1);
    while (!encoded_value.empty() && encoded_value[0] != 'e') {
        try {
            nlohmann::json item = decode(encoded_value);
            list.push_back(item);
        } catch(const std::exception& e) {
            throw std::runtime_error("Invalid encoded list: " + encoded_value + " " + std::string(e.what()));
        }
    }

    if(encoded_value.empty() || encoded_value[0] != 'e') {
        throw std::runtime_error("Invalid encoded list: missing e" + encoded_value);
    }

    encoded_value = encoded_value.substr(1);
    return nlohmann::json(list);
}

nlohmann::json BencodeParser::parse_dictionary(std::string& encoded_value) {
    std::map<nlohmann::json, nlohmann::json> dict;
    encoded_value = encoded_value.substr(1);
    while (!encoded_value.empty() && encoded_value[0] != 'e') {
        try {
            nlohmann::json key = decode(encoded_value);
            nlohmann::json value = decode(encoded_value);
            dict[key] = value;
        } catch(const std::exception& e) {
            throw std::runtime_error("Invalid encoded dictionary: " + encoded_value + " " + std::string(e.what()));
        }
    }

    if(encoded_value.empty() || encoded_value[0] != 'e') {
        throw std::runtime_error("Invalid encoded dictionary: missing e" + encoded_value);
    }

    encoded_value = encoded_value.substr(1);
    return nlohmann::json(dict);
}



