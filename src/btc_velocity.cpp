// src/btc_velocity_live.cpp
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <chrono>
#include <thread>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// --- CURL callback ---
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    s->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// --- Data structure ---
struct DataPoint {
    std::chrono::system_clock::time_point timestamp;
    double price = 0.0;
    double pct_change = 0.0;
    std::string direction = "flat";
    double velocity_dollar_per_min = 0.0;
    double velocity_dollar_per_sec = 0.0;
};

// --- Format timestamp ---
std::string formatTime(const std::chrono::system_clock::time_point& tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm* tm = std::localtime(&t);
    char buf[24]{};
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:00", tm);  // floor to minute
    return std::string(buf);
}

// --- Fetch current BTC price from Coingecko ---
double fetchBTCPrice() {
    CURL* curl = curl_easy_init();
    std::string response;
    double price = 0.0;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK) {
            try {
                json j = json::parse(response);
                price = j["bitcoin"]["usd"].get<double>();
            } catch (...) {
                std::cerr << "JSON parse error\n";
            }
        }
    }
    return price;
}

// --- Main ---
int main() {
    const:cout << "BTC Live Velocity Generator STARTED\n";
    std::vector<DataPoint> history;
    const std::string output_file = "btc_velocity.csv";

    // Initialize CSV with header if not exists
    if (!std::filesystem::exists(output_file)) {
        std::ofstream out(output_file);
        out << "timestamp,price,pct_change,direction,velocity_dollar_per_min,velocity_dollar_per_sec\n";
        out.close();
    }

    while (true) {
        auto now = std::chrono::system_clock::now();
        auto now_min = std::chrono::floor<std::chrono::minutes>(now);
        std::string time_str = formatTime(now_min);

        double current_price = fetchBTCPrice();
        if (current_price <= 0) {
            std::cerr << "Failed to fetch price, retry in 60s...\n";
            std::this_thread::sleep_for(std::chrono::seconds(60));
            continue;
        }

        DataPoint dp;
        dp.timestamp = now_min;
        dp.price = current_price;

        // Compute metrics if we have previous data
        if (!history.empty()) {
            const auto& prev = history.back();
            double dollar_change = dp.price - prev.price;
            double minutes_diff = 1.0;  // we run every minute

            if (prev.price != 0.0)
                dp.pct_change = (dollar_change / prev.price) * 100.0;

            dp.direction = (dollar_change > 0) ? "up" : (dollar_change < 0) ? "down" : "flat";
            dp.velocity_dollar_per_min = std::abs(dollar_change) / minutes_diff;
            dp.velocity_dollar_per_sec = dp.velocity_dollar_per_min / 60.0;
        } else {
            dp.pct_change = dp.velocity_dollar_per_min = dp.velocity_dollar_per_sec = 0.0;
            dp.direction = "flat";
        }

        // Append only if new minute
        if (history.empty() || history.back().timestamp != dp.timestamp) {
            history.push_back(dp);

            // Keep only last 100 minutes
            if (history.size() > 100) history.erase(history.begin());

            // Write to CSV
            std::ofstream out(output_file, std::ios::app);
            out << std::fixed << std::setprecision(4);
            out << time_str << ','
                << dp.price << ','
                << (history.size() == 1 ? "" : std::to_string(dp.pct_change)) << ','
                << dp.direction << ','
                << dp.velocity_dollar_per_min << ','
                << dp.velocity_dollar_per_sec << '\n';
            out.close();

            // PRINT THE NEW LINE TO TERMINAL
            std::cout << "[LIVE] " << time_str
                      << " | $" << dp.price
                      << " | " << (dp.pct_change == 0 && history.size() == 1 ? "NaN" : std::to_string(dp.pct_change))
                      << "% | " << dp.direction
                      << " | vel: $" << dp.velocity_dollar_per_min << "/min\n";
        }

        // Wait until next minute
        auto next_min = now_min + std::chrono::minutes(1);
        std::this_thread::sleep_until(next_min);
    }

    return 0;
}