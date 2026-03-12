cmake_minimum_required(VERSION 3.14)
project(nuclaw_step00 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Step 0 只用 Boost.System
find_package(Boost 1.70 REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)

add_executable(nuclaw_step00 main.cpp)

target_link_libraries(nuclaw_step00 
    Boost::system 
    Threads::Threads
)
