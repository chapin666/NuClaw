# ============================================================================
# Step 10 CMakeLists.txt - RAG Retrieval
# ============================================================================

cmake_minimum_required(VERSION 3.14)
project(nuclaw_step10 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.70 REQUIRED COMPONENTS system thread json)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

set(SOURCES
    main.cpp
)

add_executable(nuclaw_step10 ${SOURCES})

target_link_libraries(nuclaw_step10 
    Boost::system 
    Boost::thread
    Boost::json
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_include_directories(nuclaw_step10 PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
