cmake_minimum_required(VERSION 3.13)
project(bittorrent-starter-cpp)

set(CMAKE_CXX_STANDARD 20) # Enable the C++20 standard

# Find OpenSSL package
find_package(OpenSSL REQUIRED)

# Find CURL package
find_package(CURL REQUIRED)

# Find pthread package
find_package(Threads REQUIRED)

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
    src/commands/HandshakeCommand.cpp
    src/commands/DownloadPieceCommand.cpp
    src/commands/DownloadCommand.cpp
    src/commands/MagnetParseCommand.cpp
    src/commands/MagnetHandshakeCommand.cpp
    src/commands/MagnetInfoCommand.cpp
    src/commands/MagnetDownloadPieceCommand.cpp
    src/commands/MagnetDownloadCommand.cpp
    src/manager/CommandManager.cpp
    src/manager/PeerManager.cpp
    src/manager/PieceManager.cpp
    src/bencode/BencodeDecoder.cpp
    src/bencode/BencodeEncoder.cpp
    src/utils/SHA1.cpp
    src/utils/TorrentUtils.cpp
    src/utils/PeerUtils.cpp
    src/utils/MagnetUtils.cpp
    src/protocol/PeerMessage.cpp
)

# Collect all header files (optional but good for IDE integration)
set(HEADER_FILES
    src/commands/Command.hpp
    src/commands/DecodeCommand.hpp
    src/commands/InfoCommand.hpp
    src/commands/PeersCommand.hpp
    src/commands/HandshakeCommand.hpp
    src/commands/DownloadPieceCommand.hpp
    src/commands/DownloadCommand.hpp
    src/commands/MagnetParseCommand.hpp
    src/commands/MagnetHandshakeCommand.hpp
    src/commands/MagnetInfoCommand.hpp
    src/commands/MagnetDownloadPieceCommand.hpp
    src/commands/MagnetDownloadCommand.hpp
    src/manager/CommandManager.hpp
    src/manager/PeerManager.hpp
    src/manager/PieceManager.hpp
    src/bencode/BencodeDecoder.hpp
    src/bencode/BencodeEncoder.hpp
    src/bencode/Bencode.hpp
    src/utils/SHA1.hpp
    src/utils/TorrentUtils.hpp
    src/utils/PeerUtils.hpp
    src/utils/MagnetUtils.hpp
    src/lib/nlohmann/json.hpp
    src/protocol/PeerMessage.hpp
    src/protocol/PeerMessageType.hpp
)

# Create executable
add_executable(bittorrent 
    ${SOURCE_FILES}
    ${HEADER_FILES}
)

# Link OpenSSL, CURL, and pthread libraries
target_link_libraries(bittorrent 
    OpenSSL::Crypto  # We only need the crypto part of OpenSSL for SHA1
    CURL::libcurl     # Link against the CURL library
    Threads::Threads   # Link against the pthread library
)
# Add compiler warnings
if(MSVC)
    target_compile_options(bittorrent PRIVATE /W4)
else()
    target_compile_options(bittorrent PRIVATE -Wall -Wextra)
endif()

