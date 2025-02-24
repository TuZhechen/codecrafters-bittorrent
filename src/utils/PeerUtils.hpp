#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "../protocol/PeerMessageType.hpp"

class PeerUtils {
public:
    explicit PeerUtils(int socket_fd) : sock(socket_fd) {}
    
    void receiveMessage(unsigned char* msg_length_buf, char& msg_type, std::vector<uint8_t>& payload);
    void sendMessage(PeerMessageType msg_type, const std::vector<uint8_t>& payload);
    
    // This could be static as it doesn't depend on socket
    static void addIntToPayload(std::vector<uint8_t>& payload, int value, int offset);
private:
    int sock; 
};   