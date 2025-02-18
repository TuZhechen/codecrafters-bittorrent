#pragma once

#include <string>
#include <vector>

class Command {
public:
    virtual ~Command() = default;
    virtual void execute(const std::vector<std::string>& args) = 0;
}; 