cmake_minimum_required(VERSION 3.14)
project(nuclaw-common-test VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_package(Boost REQUIRED COMPONENTS system thread json)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# 查找 libpqxx
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

# 头文件搜索路径
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
    ${Boost_INCLUDE_DIRS}
)

# 打印配置
message(STATUS "========================================")
message(STATUS "NuClaw Build Configuration")
message(STATUS "========================================")
message(STATUS "Boost Version: ${Boost_VERSION}")
message(STATUS "OpenSSL Version: ${OPENSSL_VERSION}")
message(STATUS "libpqxx: ${PQXX_VERSION}")
message(STATUS "")
message(STATUS "Build individual steps:")
message(STATUS "  cd src/step06 && mkdir build && cd build && cmake .. && make")
message(STATUS "========================================")
