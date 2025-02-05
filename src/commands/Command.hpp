#pragma once

#include <string>

class Command {
public:
    virtual ~Command() = default;
    virtual void execute(const std::string& input) = 0;
}; 