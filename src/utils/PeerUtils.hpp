#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "../protocol/PeerMessageType.hpp"

class PeerUtils {
public:
    explicit PeerUtils(int socket_fd) : sock(socket_fd) {}
    
    static std::pair<std::string, int> parsePeerAddress(const std::string& peer_addr);
    static std::pair<std::string, int> parsePeerAddress(const std::string& peers_data, int offset);
    void receiveMessage(unsigned char* msg_length_buf, char& msg_type, std::vector<uint8_t>& payload);
    void sendMessage(PeerMessageType msg_type, const std::vector<uint8_t>& payload);
    
    // This could be static as it doesn't depend on socket
    static void addIntToPayload(std::vector<uint8_t>& payload, int value, int offset);
private:
    int sock; 
};   