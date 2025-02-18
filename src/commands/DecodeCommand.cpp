#include "DecodeCommand.hpp"
#include <iostream>

void DecodeCommand::execute(const std::vector<std::string>& args) {
    std::string encoded_value = args[0];
    nlohmann::json decoded_value = decoder.decode(encoded_value);
    std::cout << decoded_value.dump() << std::endl;

} 