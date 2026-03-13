cmake_minimum_required(VERSION 3.14)
project(nuclaw_step12 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system json thread)
find_package(Threads REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${Boost_INCLUDE_DIRS})

add_executable(nuclaw_step12
    src/main.cpp
)

target_link_libraries(nuclaw_step12
    ${Boost_LIBRARIES}
    Threads::Threads
)
