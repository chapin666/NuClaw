# ============================================================================
# Step 8 CMakeLists.txt - Tool Ecosystem
# ============================================================================

cmake_minimum_required(VERSION 3.14)
project(nuclaw_step08 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.70 REQUIRED COMPONENTS system thread json)
find_package(Threads REQUIRED)

set(SOURCES
    main.cpp
    server.cpp
)

add_executable(nuclaw_step08 ${SOURCES})

target_link_libraries(nuclaw_step08 
    Boost::system 
    Boost::thread
    Boost::json
    Threads::Threads
)

target_include_directories(nuclaw_step08 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
