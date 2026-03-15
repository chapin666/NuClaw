# ============================================================================
# Step 6 CMakeLists.txt - Tool Calling (Modular)
# ============================================================================

cmake_minimum_required(VERSION 3.14)
project(nuclaw_step06 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.70 REQUIRED COMPONENTS system thread json)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

set(SOURCES
    main.cpp
)

add_executable(nuclaw_step06 ${SOURCES})

target_link_libraries(nuclaw_step06 
    Boost::system 
    Boost::thread
    Boost::json
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_include_directories(nuclaw_step06 PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
