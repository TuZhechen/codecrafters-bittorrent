#include "SHA1.hpp"
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>

std::array<unsigned char, 20> SHA1::calculate(const std::string& input) {
    std::array<unsigned char, 20> hash;
    SHA_CTX context;
    SHA1_Init(&context);
    SHA1_Update(&context, input.c_str(), input.length());
    SHA1_Final(hash.data(), &context);
    return hash;
}

std::string SHA1::toHex(const std::array<unsigned char, 20>& hash) {
    std::stringstream ss;
    for(unsigned char byte : hash) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return ss.str();
}