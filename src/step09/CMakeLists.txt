cmake_minimum_required(VERSION 3.14)
project(nuclaw_step09)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 Boost
find_package(Boost REQUIRED COMPONENTS system json)
find_package(Threads REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})

# 可执行文件
add_executable(nuclaw_step09
    main.cpp
)

target_link_libraries(nuclaw_step09
    ${Boost_LIBRARIES}
    Threads::Threads
)
