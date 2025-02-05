cmake_minimum_required(VERSION 3.13)
project(bittorrent-starter-cpp)

set(CMAKE_CXX_STANDARD 20) # Enable the C++20 standard

# Add include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/lib
)

# Collect all source files
set(SOURCE_FILES
    src/Main.cpp
    src/commands/DecodeCommand.cpp
    src/commands/InfoCommand.cpp
    src/manager/CommandManager.cpp
    src/bencode/BencodeParser.cpp
)

# Collect all header files (optional but good for IDE integration)
set(HEADER_FILES
    src/commands/Command.hpp
    src/commands/DecodeCommand.hpp
    src/commands/InfoCommand.hpp
    src/manager/CommandManager.hpp
    src/bencode/BencodeParser.hpp
    src/lib/nlohmann/json.hpp
)

# Create executable
add_executable(bittorrent 
    ${SOURCE_FILES}
    ${HEADER_FILES}
)

# Add compiler warnings
if(MSVC)
    target_compile_options(bittorrent PRIVATE /W4)
else()
    target_compile_options(bittorrent PRIVATE -Wall -Wextra)
endif()
