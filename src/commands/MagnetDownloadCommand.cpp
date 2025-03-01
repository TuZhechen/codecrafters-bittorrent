#include "MagnetDownloadCommand.hpp"
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void MagnetDownloadCommand::execute(const CommandOptions& options) {
    try {
        if (options.args.size() != 1) {
            throw std::runtime_error("Expected: <magnet_link>");
        }
        if (!options.options.contains("-o")) {
            throw std::runtime_error("Expected: -o <output_file>");
        }
        
        std::string magnet_link = options.args[0];
        std::string output_file = options.options.at("-o");
        
        // Parse magnet link
        nlohmann::json magnet_data = MagnetUtils::parseMagnetLink(magnet_link);
        infoHash = magnet_data["info_hash"];
        binaryInfoHash = magnet_data["binary_info_hash"];
        std::string trackerUrl = magnet_data["tracker_url"];

        // Connect to peers and start download
        connectToPeers(trackerUrl);
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

void MagnetDownloadCommand::connectToPeers(const std::string& trackerUrl) {
    // Get peers from tracker
    std::string tracker_response = MagnetUtils::makeTrackerRequest(
        trackerUrl, 
        binaryInfoHash
    );

    nlohmann::json resp_data = Bencode::decode(tracker_response);
    std::string peers_data = resp_data["peers"].get<std::string>();

    int sock = -1;

    // Handshake with each peer, request metadata and maintain peers
    for (size_t i = 0; i < peers_data.length(); i += 6) {
        auto [ip, port] = PeerUtils::parsePeerAddress(peers_data, i);
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        // Connect to peer i
        struct sockaddr_in peer_addr_in;
        peer_addr_in.sin_family = AF_INET;
        peer_addr_in.sin_port = htons(port);
        if (inet_pton(AF_INET, ip.c_str(), &peer_addr_in.sin_addr) <= 0) {
            throw std::runtime_error("Invalid IP address");
        }

        if (connect(sock, (struct sockaddr*)&peer_addr_in, sizeof(peer_addr_in)) < 0) {
            throw std::runtime_error("Failed to connect to peer");
        }

        // Perform handshake
        auto [extension_id, bitfield] = MagnetUtils::performHandshake(sock, binaryInfoHash, true);

        // Request metadata
        MagnetUtils::requestMetadata(sock, extension_id);

        // Receive metadata
        nlohmann::json metadata = MagnetUtils::receiveMetadata(sock, infoHash);
        if(!metadata.is_null()) {
            if (piece_manager == nullptr) { 
                int file_length = metadata["length"].get<int>();
                int piece_length = metadata["piece length"].get<int>();
                int total_pieces = (file_length + piece_length - 1) / piece_length;
                std::string pieces_hash = metadata["pieces"].get<std::string>();

                // Initialize piece manager
                piece_manager = std::make_unique<PieceManager>(
                    total_pieces, piece_length, file_length, infoHash, pieces_hash
                );
            }

            // Initialize peer
            auto peer = std::make_unique<PeerManager>(ip, port, infoHash);
            if (peer -> magnetConnect(sock, bitfield)) {
                peers.push_back(std::move(peer));
            }
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

void MagnetDownloadCommand::downloadAllPieces() {
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
