#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <chrono>
#include <thread>
#include "BoardShim.h"
#include "DataFilter.h"

struct DataPoint {
    std::chrono::system_clock::time_point timestamp;
    double value = 0.0;  // Avg EEG µV across channels
    double change = 0.0;
    std::string direction = "flat";
    double velocity_uv_per_min = 0.0;
    double velocity_uv_per_sec = 0.0;
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <minutes_to_stream> [output_csv]" << std::endl;
        return 1;
    }
    int minutes = std::stoi(argv[1]);
    std::string output_file = (argc > 2) ? argv[2] : "muse_data.csv";

    // BrainFlow setup for Muse 2 (BOARD_ID = 7)
    BrainFlow::BoardShim::enable_dev_board_logger();
    BrainFlow::InputParams params;
    auto board = new BrainFlow::BoardShim(BrainFlow::MUSE_2_BOARD, params);  // Or MUSE_S_BOARD if S model
    try {
        board->prepare_session();
        board->start_stream(45000);  // 45k buffer for 256Hz * 60s * mins

        // Stream loop: Collect data every second, aggregate to minutes
        std::vector<DataPoint> data;
        auto start_time = std::chrono::system_clock::now();
        auto last_min_time = start_time;

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            auto current_time = std::chrono::system_clock::now();
            double elapsed_min = std::chrono::duration<double>(current_time - start_time).count() / 60.0;

            if (elapsed_min >= minutes) break;

            // Get recent data (last second)
            auto recent_data = board->get_current_board_data(256);  // 256 samples/sec
            if (recent_data.size() == 0) continue;

            // EEG channels for Muse: 0=TP9, 1=AF7, 2=AF8, 3=TP10
            double sum_uv = 0.0;
            int valid_samples = 0;
            auto eeg_chans = BrainFlow::BoardShim::get_eeg_channels(BrainFlow::MUSE_2_BOARD);
            for (int i = 0; i < recent_data.size(); ++i) {
                for (int ch : eeg_chans) {
                    if (std::isfinite(recent_data[ch][i])) {
                        sum_uv += recent_data[ch][i];  // µV
                        ++valid_samples;
                    }
                }
            }
            double avg_uv = (valid_samples > 0) ? sum_uv / valid_samples : 0.0;

            // New minute? Aggregate and push
            if (std::chrono::duration<double>(current_time - last_min_time).count() >= 60.0) {
                DataPoint dp;
                dp.timestamp = last_min_time;  // Floor to minute
                dp.value = avg_uv;  // Or use BrainFlow::DataFilter for power bands, e.g., alpha
                data.push_back(dp);
                last_min_time = current_time;
            }
        }

        board->stop_stream();
        board->release_session();
        delete board;

        // Compute metrics (like BTC script)
        if (data.size() < 2) {
            std::cerr << "Need at least 2 points for changes." << std::endl;
            return 1;
        }
        for (size_t i = 1; i < data.size(); ++i) {
            auto& curr = data[i];
            const auto& prev = data[i - 1];

            double uv_change = curr.value - prev.value;
            curr.change = uv_change;

            if (uv_change > 0) curr.direction = "up";
            else if (uv_change < 0) curr.direction = "down";
            else curr.direction = "flat";

            double minutes_diff = 1.0;  // Fixed 1-min intervals
            curr.velocity_uv_per_min = std::abs(uv_change) / minutes_diff;
            curr.velocity_uv_per_sec = curr.velocity_uv_per_min / 60.0;
        }

        // First row: NaN-like
        data[0].change = 0.0;
        data[0].direction = "flat";
        data[0].velocity_uv_per_min = 0.0;
        data[0].velocity_uv_per_sec = 0.0;

        // Write CSV
        std::ofstream out(output_file);
        out << std::fixed << std::setprecision(4);
        out << "timestamp,value,change,direction,velocity_uv_per_min,velocity_uv_per_sec\n";
        for (const auto& dp : data) {
            auto time_t = std::chrono::system_clock::to_time_t(dp.timestamp);
            std::tm* tm = std::localtime(&time_t);
            char buffer[20];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:00", tm);  // Minute floor

            out << buffer << ","
                << dp.value << ","
                << (dp.change == 0.0 && &dp == &data[0] ? "" : std::to_string(dp.change)) << ","
                << dp.direction << ","
                << dp.velocity_uv_per_min << ","
                << dp.velocity_uv_per_sec << "\n";
        }
        out.close();

        std::cout << "CSV generated: " << output_file << " (" << data.size() << " minutes)" << std::endl;

        // Console preview (first 5)
        std::cout << "\n--- Preview ---\n";
        std::cout << std::left << std::setw(20) << "timestamp" << std::setw(10) << "value(µV)" << std::setw(10) << "change" << std::setw(10) << "dir" << std::setw(15) << "vel µV/min" << std::setw(15) << "vel µV/sec" << "\n";
        for (size_t i = 0; i < std::min(data.size(), size_t(5)); ++i) {
            const auto& dp = data[i];
            auto time_t = std::chrono::system_clock::to_time_t(dp.timestamp);
            std::tm* tm = std::localtime(&time_t);
            char buffer[20];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:00", tm);

            std::string chg_str = (i == 0) ? "NaN" : std::to_string(dp.change);
            std::string vel_min_str = (i == 0) ? "NaN" : std::to_string(dp.velocity_uv_per_min);
            std::string vel_sec_str = (i == 0) ? "NaN" : std::to_string(dp.velocity_uv_per_sec);

            std::cout << std::left << std::setw(20) << buffer << std::setw(10) << dp.value << std::setw(10) << chg_str << std::setw(10) << dp.direction << std::setw(15) << vel_min_str << std::setw(15) << vel_sec_str << "\n";
        }

    } catch (const BrainFlow::BrainFlowException& err) {
        std::cerr << "BrainFlow error: " << err.what() << std::endl;
        return 1;
    }

    return 0;
}