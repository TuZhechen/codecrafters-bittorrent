#pragma once
#include <string>
#include <vector>
#include <queue>
#include <set>
#include <map>
#include <mutex>
#include <condition_variable>
#include <algorithm>

class PieceManager {
public:
    PieceManager(int total_pieces, int piece_length, int file_length, 
                 const std::string& info_hash, const std::string& pieces_hash);
    
    bool isDownloadComplete() const;
    int getNextPiece();  // Thread-safe piece selection
    bool savePieceData(int index, const std::vector<uint8_t>& data);
    bool verifyPiece(int index, const std::vector<uint8_t>& data) const;
    bool verifyFullFile() const;
    bool writeToFile(const std::string& output_path) const;
    int getPieceLength(int index) const;
    int getFileLength() const { return file_length; }
    int getTotalPieces() const { return total_pieces; }
    int getCompletedPieces() const {
        return std::count_if(pieces.begin(), pieces.end(),
            [](const auto& p) { return p.second.state == PieceInfo::COMPLETED; });
    }

private:
    struct PieceInfo {
        enum State { PENDING, DOWNLOADING, COMPLETED };
        State state = PENDING;
        std::vector<uint8_t> data;
        bool verified = false;
    };

    mutable std::mutex piece_mutex;
    std::condition_variable piece_cv;
    std::queue<int> pending_pieces;
    std::set<int> downloading_pieces;
    std::map<int, PieceInfo> pieces;
    const int total_pieces;
    const int piece_length;
    const int file_length;
    const std::string pieces_hash;
    const std::string info_hash;
};