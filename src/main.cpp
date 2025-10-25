// src/main.cpp
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <string>
#include <mutex>

#include "board_shim.h"
#include <crow.h>

using namespace std;
using namespace std::chrono_literals;

atomic<bool> streaming(false);
mutex data_mutex;
vector<double> latest_eeg;

void stream_muse() {
    BoardShim::enable_dev_board_logger();
    struct BrainFlowInputParams params;
    int board_id = (int)BoardIds::MUSE_S_BOARD;

    BoardShim* board = new BoardShim(board_id, params);

    try {
        board->prepare_session();
        board->config_board("p61");  // High-res mode
        board->start_stream();

        streaming = true;
        cout << "[Muse] Streaming started...\n";

        auto eeg_channels = BoardShim::get_eeg_channels(board_id);

        while (streaming) {
            this_thread::sleep_for(100ms);
            auto data = board->get_current_board_data(1);

            if (data.empty()) continue;

            lock_guard<mutex> lock(data_mutex);
            latest_eeg.clear();
            int cols = BoardShim::get_num_channels(board_id);
            for (int ch : eeg_channels) {
                latest_eeg.push_back(data[ch]);
            }
        }

        board->stop_stream();
        board->release_session();
        cout << "[Muse] Stopped.\n";
    }
    catch (const BrainFlowException& err) {
        BoardShim::log_message((int)LogLevels::LEVEL_ERROR, err.what());
        if (board->is_prepared()) board->release_session();
    }
    delete board;
}

int main() {
    crow::SimpleApp app;

    thread muse_thread(stream_muse);

    CROW_ROUTE(app, "/")([](){
        return "Muse EEG Server Running";
    });

    CROW_ROUTE(app, "/api/eeg/latest")
    .methods("GET"_method)
    ([&]() {
        lock_guard<mutex> lock(data_mutex);
        if (latest_eeg.empty()) {
            return crow::response(503, "No data yet");
        }

        crow::json::wvalue result;
        vector<string> names = {"TP9", "AF7", "AF8", "TP10"};
        for (size_t i = 0; i < latest_eeg.size(); i++) {
            result[names[i]] = latest_eeg[i];
        }
        return crow::response(result);
    });

    CROW_ROUTE(app, "/api/stop")
    .methods("POST"_method)
    ([&]() {
        streaming = false;
        muse_thread.join();
        return "Streaming stopped.";
    });

    cout << "Server: http://localhost:8080\n";
    app.port(8080).multithreaded().run();
}