cmake_minimum_required(VERSION 3.14)
project(nuclaw VERSION 1.0.0 LANGUAGES CXX)

# C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)

# 查找依赖
find_package(Boost 1.70 REQUIRED COMPONENTS system json thread)
find_package(Threads REQUIRED)

# 头文件目录
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${Boost_INCLUDE_DIRS})

# 源文件
set(NUCLAW_SOURCES
    src/main.cpp
)

# 可执行文件
add_executable(nuclaw ${NUCLAW_SOURCES})

target_link_libraries(nuclaw
    ${Boost_LIBRARIES}
    Threads::Threads
)

# 安装目标
install(TARGETS nuclaw DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)

# 测试
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# 示例
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
