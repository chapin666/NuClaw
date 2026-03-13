# ============================================================================
# NuClaw Step 2 - HTTP Protocol
# ============================================================================

cmake_minimum_required(VERSION 3.14)
project(nuclaw_step02 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.70 REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)

add_executable(nuclaw_step02 main.cpp)

target_link_libraries(nuclaw_step02 
    Boost::system 
    Boost::thread
    Threads::Threads
)
