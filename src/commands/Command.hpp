#pragma once

#include <string>
#include <vector>
#include "CommandOptions.hpp"

class Command {
public:
    virtual ~Command() = default;
    virtual void execute(const CommandOptions& options) = 0;
}; 