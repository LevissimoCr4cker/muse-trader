cmake_minimum_required(VERSION 3.14)
project(muse-trader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------
# 1. BrainFlow – point to the *installed* location (install/lib/cmake/BrainFlow)
# ------------------------------------------------------------------
set(BrainFlow_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/brainflow/build/install/lib/cmake/BrainFlow)
find_package(BrainFlow REQUIRED)

# ------------------------------------------------------------------
# 2. Crow (web server)
# ------------------------------------------------------------------
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../external/Crow ${CMAKE_BINARY_DIR}/crow)

# ------------------------------------------------------------------
# 3. nlohmann-json header (single-file, no library)
# ------------------------------------------------------------------
set(JSON_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# ------------------------------------------------------------------
# 4. Executables
# ------------------------------------------------------------------
add_executable(muse_server          main.cpp)
add_executable(btc_velocity_live   btc_velocity_live.cpp)
add_executable(muse_brainflow_streamer muse_brainflow_streamer.cpp)

# ------------------------------------------------------------------
# 5. Link libraries
# ------------------------------------------------------------------
target_link_libraries(muse_server
    PRIVATE BrainFlow::BrainFlow Crow::Crow pthread
)

target_link_libraries(btc_velocity_live
    PRIVATE curl pthread
)

target_link_libraries(muse_brainflow_streamer
    PRIVATE BrainFlow::BrainFlow pthread
)

# ------------------------------------------------------------------
# 6. Include directories (only needed for btc_velocity_live)
# ------------------------------------------------------------------
target_include_directories(btc_velocity_live PRIVATE ${JSON_INCLUDE})

# ------------------------------------------------------------------
# 7. Windows extras
# ------------------------------------------------------------------
if(WIN32)
    target_link_libraries(muse_server          PRIVATE ws2_32 wsock32)
    target_link_libraries(btc_velocity_live   PRIVATE ws2_32 wsock32 curl)
    target_link_libraries(muse_brainflow_streamer PRIVATE ws2_32 wsock32)
endif()