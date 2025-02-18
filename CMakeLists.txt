cmake_minimum_required(VERSION 3.13)
project(bittorrent-starter-cpp)

set(CMAKE_CXX_STANDARD 20) # Enable the C++20 standard

# Find OpenSSL package
find_package(OpenSSL REQUIRED)

# Find CURL package
find_package(CURL REQUIRED)

# Add include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/lib
    ${OPENSSL_INCLUDE_DIR}  # Add OpenSSL include directory
    ${CURL_INCLUDE_DIR}      # Add CURL include directory
)

# Collect all source files
set(SOURCE_FILES
    src/Main.cpp
    src/commands/DecodeCommand.cpp
    src/commands/InfoCommand.cpp
    src/commands/PeersCommand.cpp
    src/manager/CommandManager.cpp
    src/bencode/BencodeDecoder.cpp
    src/bencode/BencodeEncoder.cpp
    src/utils/SHA1.cpp
)

# Collect all header files (optional but good for IDE integration)
set(HEADER_FILES
    src/commands/Command.hpp
    src/commands/DecodeCommand.hpp
    src/commands/InfoCommand.hpp
    src/commands/PeersCommand.hpp
    src/manager/CommandManager.hpp
    src/bencode/BencodeDecoder.hpp
    src/bencode/BencodeEncoder.hpp
    src/bencode/Bencode.hpp
    src/utils/SHA1.hpp
    src/lib/nlohmann/json.hpp
)

# Create executable
add_executable(bittorrent 
    ${SOURCE_FILES}
    ${HEADER_FILES}
)

# Link OpenSSL and CURL libraries
target_link_libraries(bittorrent 
    OpenSSL::Crypto  # We only need the crypto part of OpenSSL for SHA1
    CURL::libcurl     # Link against the CURL library
)

# Add compiler warnings
if(MSVC)
    target_compile_options(bittorrent PRIVATE /W4)
else()
    target_compile_options(bittorrent PRIVATE -Wall -Wextra)
endif()
