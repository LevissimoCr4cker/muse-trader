cmake_minimum_required(VERSION 3.14)
project(muse-trader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === BRAIN FLOW ===
set(BrainFlow_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/brainflow/build/install/lib/cmake/BrainFlow)
find_package(BrainFlow REQUIRED)

# === CROW ===
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../external/Crow ${CMAKE_BINARY_DIR}/crow)

# === EXECUTABLES ===
add_executable(muse_server main.cpp)
add_executable(btc_velocity_live btc_velocity_live.cpp)

target_link_libraries(muse_server
    PRIVATE
        BrainFlow::BrainFlow
        Crow::Crow
        pthread
)

target_link_libraries(btc_velocity_live
    PRIVATE
        curl
        pthread
)

# === JSON HEADER ===
target_include_directories(btc_velocity_live PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

if(WIN32)
    target_link_libraries(muse_server PRIVATE ws2_32 wsock32)
    target_link_libraries(btc_velocity_live PRIVATE ws2_32 wsock32 curl)
endif()