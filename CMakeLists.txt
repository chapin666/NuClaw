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
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
)

# 测试 HTTP 组件
add_executable(test_http_components test_http_components.cpp)

target_link_libraries(test_http_components
    Boost::system
    Boost::thread
    Boost::json
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

# 测试数据库组件
add_executable(test_database test_database.cpp)

target_include_directories(test_database PRIVATE ${PQXX_INCLUDE_DIRS})
target_compile_options(test_database PRIVATE ${PQXX_CFLAGS_OTHER})

target_link_libraries(test_database
    ${PQXX_LIBRARIES}
    Threads::Threads
)

# 打印配置
message(STATUS "========================================")
message(STATUS "NuClaw Common Components Test")
message(STATUS "========================================")
message(STATUS "Boost Version: ${Boost_VERSION}")
message(STATUS "OpenSSL Version: ${OPENSSL_VERSION}")
message(STATUS "libpqxx: ${PQXX_VERSION}")
message(STATUS "========================================")
