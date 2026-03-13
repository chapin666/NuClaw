# ============================================================================
# NuClaw Step 4 - WebSocket
# ============================================================================

cmake_minimum_required(VERSION 3.14)
project(nuclaw_step04 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.70 REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)

add_executable(nuclaw_step04 main.cpp)

target_link_libraries(nuclaw_step04 
    Boost::system 
    Boost::thread
    Threads::Threads
)
