#include "DownloadCommand.hpp"
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

void DownloadCommand::execute(const CommandOptions& options) {
    try {
        if (options.args.size() != 1) {
            throw std::runtime_error("Expected: <torrent_file>");
        }
        
        std::string torrent_file = options.args[0];
        std::string output_file = options.options.at("-o");
        
        // Parse torrent file
        std::string torrent_content = TorrentUtils::readTorrentFile(torrent_file);
        BencodeDecoder decoder;
        nlohmann::json torrent_data = decoder.decode(torrent_content);
        
        // Get piece info
        const auto& info = torrent_data["info"];
        int piece_length = info["piece length"];
        int64_t file_length = info["length"].get<int64_t>();
        std::string pieces_hash = info["pieces"].get<std::string>();
        int total_pieces = (file_length + piece_length - 1) / piece_length;

        // Calculate info hash
        BencodeEncoder encoder;
        std::string encoded_info = encoder.encode(info);
        auto hash = SHA1::calculate(encoded_info);
        info_hash = std::string(reinterpret_cast<char*>(hash.data()), 20);

        // Initialize piece manager
        piece_manager = std::make_unique<PieceManager>(
            total_pieces, piece_length, file_length, info_hash, pieces_hash
        );

        // Connect to peers and start download
        connectToPeers(torrent_data["announce"].get<std::string>());
        downloadAllPieces();

        // Verify and save file
        if (!piece_manager->verifyFullFile()) {
            throw std::runtime_error("File verification failed");
        }

        if (!piece_manager->writeToFile(output_file)) {
            throw std::runtime_error("Failed to write output file");
        }

    } catch (const std::exception& e) {
        throw std::runtime_error("Download failed: " + std::string(e.what()));
    }
}

void DownloadCommand::connectToPeers(const std::string& announce_url) {
    // Get peers from tracker
    std::string tracker_response = TorrentUtils::makeTrackerRequest(
        announce_url, info_hash, piece_manager->getFileLength()
    );

    BencodeDecoder decoder;
    nlohmann::json resp_data = decoder.decode(tracker_response);
    std::string peers_data = resp_data["peers"].get<std::string>();

    // Connect to peers
    for (size_t i = 0; i < peers_data.length(); i += 6) {
        auto [ip_str, port] = PeerUtils::parsePeerAddress(peers_data, i);

        auto peer = std::make_unique<PeerManager>(ip_str, port, info_hash);
        if (peer->connect()) {
            peers.push_back(std::move(peer));
        }
    }

    if (peers.empty()) {
        throw std::runtime_error("No peers available");
    }
}

class DownloadWorker {
private:
    struct PieceTask {
        int index;
        std::vector<uint8_t> data;
    };

    // Thread-safe queue for completed pieces waiting to be saved
    class SaveQueue {
    public:
        void push(PieceTask task) {
            std::lock_guard<std::mutex> lock(mutex);
            queue.push(std::move(task));
            cv.notify_one();
        }

        bool pop(PieceTask& task) {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [this] { return !queue.empty() || stop_flag; });
            if (stop_flag && queue.empty()) return false;
            task = std::move(queue.front());
            queue.pop();
            return true;
        }

        void stop() {
            std::lock_guard<std::mutex> lock(mutex);
            stop_flag = true;
            cv.notify_all();
        }

    private:
        std::queue<PieceTask> queue;
        std::mutex mutex;
        std::condition_variable cv;
        bool stop_flag = false;
    };

    void downloadLoop() {
        while (running) {
            int next_piece = piece_manager->getNextPiece();
            if (next_piece == -1) break;

            if (!peer->hasPiece(next_piece)) {
                piece_manager->savePieceData(next_piece, {});
                continue;
            }

            std::cout << "Worker " << peer->getPeerInfo() 
                      << " start piece " << next_piece << std::endl;
            
            std::vector<uint8_t> piece_data;
            int piece_length = piece_manager->getPieceLength(next_piece);

            if (peer->downloadPiece(next_piece, piece_length, piece_data)) {
                std::cout << "Worker " << peer->getPeerInfo() 
                          << " finish piece " << next_piece 
                          << " (size: " << piece_data.size() << ")" << std::endl;
                
                // Push to save queue and continue downloading
                save_queue.push({next_piece, std::move(piece_data)});
            } else {
                piece_manager->savePieceData(next_piece, {});
            }
        }
    }

    void saveLoop() {
        PieceTask task;
        while (save_queue.pop(task)) {
            if (piece_manager->savePieceData(task.index, task.data)) {
                std::cout << "Successfully saved piece " << task.index << std::endl;
            } else {
                std::cout << "Failed to save piece " << task.index << std::endl;
            }
        }
    }

public:
    DownloadWorker(PeerManager* peer, PieceManager* piece_mgr) 
        : peer(peer), piece_manager(piece_mgr), running(true) {}

    void start() {
        download_thread = std::thread(&DownloadWorker::downloadLoop, this);
        save_thread = std::thread(&DownloadWorker::saveLoop, this);
    }

    void stop() {
        running = false;
        save_queue.stop();
        if (download_thread.joinable()) download_thread.join();
        if (save_thread.joinable()) save_thread.join();
    }

private:
    PeerManager* peer;
    PieceManager* piece_manager;
    std::thread download_thread;
    std::thread save_thread;
    std::atomic<bool> running;
    SaveQueue save_queue;
};

void DownloadCommand::downloadAllPieces() {
    std::vector<std::unique_ptr<DownloadWorker>> workers;
    
    // Create and start workers
    for (auto& peer : peers) {
        if (peer->isConnected()) {
            workers.push_back(std::make_unique<DownloadWorker>(peer.get(), piece_manager.get()));
            workers.back()->start();
        }
    }

    // Wait for download to complete
    while (!piece_manager->isDownloadComplete()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "Download complete, stopping workers" << std::endl;
    
    // Now safe to stop workers after download is complete
    for (size_t i = 0; i < workers.size(); i++) {
        std::cout << "Stopping worker " << i << "..." << std::endl;
        workers[i]->stop();
        std::cout << "Worker " << i << " stopped" << std::endl;
    }

    std::cout << "All workers stopped, proceeding to file verification" << std::endl;
}
