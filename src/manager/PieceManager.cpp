#include "PieceManager.hpp"
#include "../utils/SHA1.hpp"
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <thread>

PieceManager::PieceManager(int total_pieces, int piece_length, int file_length, 
                          const std::string& info_hash, const std::string& pieces_hash)
    : total_pieces(total_pieces), piece_length(piece_length), file_length(file_length), 
      info_hash(info_hash), pieces_hash(pieces_hash) {
    if (pieces_hash.length() != total_pieces * 20) {
        throw std::invalid_argument("Invalid pieces hash length");
    }
    for (int i = 0; i < total_pieces; ++i) {
        pending_pieces.push(i);
    }
}

bool PieceManager::isDownloadComplete() const {
    if (!pending_pieces.empty() || !downloading_pieces.empty()) {
        return false;
    }
    
    return pieces.size() == total_pieces &&
           std::all_of(pieces.begin(), pieces.end(), 
               [](const auto& p) { return p.second.state == PieceInfo::COMPLETED; });
}

int PieceManager::getNextPiece() {
    std::unique_lock<std::mutex> lock(piece_mutex);
    
    if (isDownloadComplete()) {
        piece_cv.notify_all();  // Wake up any remaining workers
        return -1;
    }
    
    piece_cv.wait(lock, [this]() {
        return !pending_pieces.empty() || isDownloadComplete();
    });

    if (isDownloadComplete()) {
        piece_cv.notify_all();  // Wake up any remaining workers
        return -1;
    }

    if (pending_pieces.empty()) {
        return -1;
    }
    
    // Get next piece if available
    int next_piece = pending_pieces.front();
    pending_pieces.pop();
    downloading_pieces.insert(next_piece);
    return next_piece;

}

bool PieceManager::savePieceData(int index, const std::vector<uint8_t>& data) {
    if (data.empty()) {
        std::lock_guard<std::mutex> lock(piece_mutex);
        downloading_pieces.erase(index);
        pending_pieces.push(index);
        piece_cv.notify_one();
        return false;
    }

    // Verify outside lock
    bool is_valid = verifyPiece(index, data);
    if (!is_valid) {
        std::lock_guard<std::mutex> lock(piece_mutex);
        downloading_pieces.erase(index);
        pending_pieces.push(index);
        piece_cv.notify_one();
        return false;
    }

    // Minimal critical section
    {
        std::lock_guard<std::mutex> lock(piece_mutex);
        pieces[index] = PieceInfo{
            .state = PieceInfo::COMPLETED,
            .data = data,
            .verified = true
        };
        downloading_pieces.erase(index);
        piece_cv.notify_one();
    }

    return true;
}

bool PieceManager::verifyPiece(int index, const std::vector<uint8_t>& data) const {
    if (index < 0 || index >= total_pieces) {
        return false;
    }
    // Calculate actual piece length (handle last piece)
    int actual_piece_length = getPieceLength(index);
    // Verify data length
    if (data.size() != actual_piece_length) {
        return false;
    }

    auto hash = SHA1::calculate(std::string(data.begin(), data.end()));
    std::string calculated_hash = std::string(reinterpret_cast<char*>(hash.data()), 20);
    // Get expected hash from pieces_hash string (20 bytes per piece)
    std::string expected_hash = pieces_hash.substr(index * 20, 20);

    return calculated_hash == expected_hash;
}

bool PieceManager::verifyFullFile() const {
    std::lock_guard<std::mutex> lock(piece_mutex);
    for (int i = 0; i < total_pieces; ++i) {
        auto it = pieces.find(i);
        if (it == pieces.end() || 
            it->second.state != PieceInfo::COMPLETED || 
            !it->second.verified || 
            !verifyPiece(i, it->second.data)) {
            return false;
        }
    }
    return true;
}

bool PieceManager::writeToFile(const std::string& output_path) const {
    std::ofstream file(output_path, std::ios::binary);
    if (!file) {
        return false;
    }

    std::lock_guard<std::mutex> lock(piece_mutex);
    for (int i = 0; i < total_pieces; i++) {
        auto it = pieces.find(i);
        if (it == pieces.end()) {
            return false;
        }
        if (!file.write(reinterpret_cast<const char*>(it->second.data.data()), it->second.data.size())) {
            return false;
        }
    }
    return true;
}

int PieceManager::getPieceLength(int index) const {
    int length = 0;
    if (index == total_pieces - 1)
        length = file_length % piece_length;
    if (length == 0) {
        length = piece_length;
    }
    return length;
}
