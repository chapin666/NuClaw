cmake_minimum_required(VERSION 3.14)
project(travelpal VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(nlohmann_json REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(travelpal
    src/main.cpp
)

target_link_libraries(travelpal
    nlohmann_json::nlohmann_json
)