#include "DecodeCommand.hpp"
#include <iostream>

void DecodeCommand::execute(const CommandOptions& options) {
    try {
        if (options.args.empty()) {
            throw std::runtime_error("No input provided for decode");
        }
        std::string encoded_value = options.args[0];
        nlohmann::json decoded_value = decoder.decode(encoded_value);
        std::cout << decoded_value.dump() << std::endl;
    } catch (const std::exception& e) {
        throw std::runtime_error("Decode failed: " + std::string(e.what()));
    }
} 