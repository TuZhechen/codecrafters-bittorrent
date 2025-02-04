#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <cstdlib>

#include "lib/nlohmann/json.hpp"

using json = nlohmann::json;

json decode_bencoded_value(std::string& encoded_value) {
    if (std::isdigit(encoded_value[0])) {
        // Parse string with length (e.g. "5:hello" -> "hello") 
        size_t colon_index = encoded_value.find(':');
        if (colon_index != std::string::npos) {
            std::string length_str = encoded_value.substr(0, colon_index);
            int64_t length = std::stoll(length_str);
            std::string content = encoded_value.substr(colon_index + 1, length);
            encoded_value = encoded_value.substr(colon_index + 1 + length);
            return json(content);

        } else {
            throw std::runtime_error("Invalid encoded string: missing colon" + encoded_value);
        }

    } else if (encoded_value[0] == 'i') {
        // Parse integer (e.g. "i-123e" -> -123)
        size_t suffix_index = encoded_value.find('e');
        if (suffix_index != std::string::npos) {
            std::string pending_value = encoded_value.substr(1, suffix_index - 1);
            try {
                int64_t value = std::stoll(pending_value);
                encoded_value = encoded_value.substr(suffix_index + 1);
                return json(value);
            } catch (std::exception& e) {
                throw std::runtime_error("Invalid encoded integer: unhandled sign" + encoded_value);
            }
        } else {
            throw std::runtime_error("Invalid encoded integer: wrong suffix" + encoded_value);
        }

    } else if (encoded_value[0] == 'l') {
        // Parse list (e.g. "l5:hello3:byei123ee" -> ["hello", "bye", 123])
        std::vector<json> list;
        encoded_value = encoded_value.substr(1); // Remove 'l' prefix
        
        // Keep parsing elements until we hit the end marker 'e'
        while (!encoded_value.empty() && encoded_value[0] != 'e') {
            try {
                json elem = decode_bencoded_value(encoded_value);
                list.push_back(elem);
            } catch (std::exception& e) {
                throw std::runtime_error("Invalid list element: " + std::string(e.what()));
            }
        }
        
        if (encoded_value.empty() || encoded_value[0] != 'e') {
            throw std::runtime_error("Invalid list: missing end marker");
        }
        
        encoded_value = encoded_value.substr(1); // Remove 'e' suffix
        return json(list);

    } else {
        throw std::runtime_error("Unhandled encoded value: " + encoded_value);
    }
}

int main(int argc, char* argv[]) {    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " decode <encoded_value>" << std::endl;
        return 1;
    }

    std::string command = argv[1];

    if (command == "decode") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " decode <encoded_value>" << std::endl;
            return 1;
        }
        std::string encoded_value = argv[2];
        json decoded_value = decode_bencoded_value(encoded_value);
        std::cout << decoded_value.dump() << std::endl;
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        return 1;
    }

    return 0;
}
