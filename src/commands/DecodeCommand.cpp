#include "DecodeCommand.hpp"
#include <iostream>

void DecodeCommand::execute(const std::string& input) {
    std::string encoded_value = input;
    nlohmann::json decoded_value = parser.decode(encoded_value);
    std::cout << decoded_value.dump() << std::endl;
} 