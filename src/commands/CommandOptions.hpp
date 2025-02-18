#pragma once
#include <string>
#include <map>
#include <vector>

struct CommandOptions {
    std::map<std::string, std::string> options;  // Stores options like -o and their values
    std::vector<std::string> args;               // Stores regular arguments
}; 