# ============================================================================
# Step 8 CMakeLists.txt - Tool Security + Database
# ============================================================================

cmake_minimum_required(VERSION 3.14)
project(nuclaw_step08 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.70 REQUIRED COMPONENTS system thread json)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# 查找 libpqxx
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX libpqxx)

set(SOURCES
    main.cpp
)

add_executable(nuclaw_step08 ${SOURCES})

target_link_libraries(nuclaw_step08 
    Boost::system 
    Boost::thread
    Boost::json
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

# 如果 libpqxx 可用，链接它
if(PQXX_FOUND)
    target_include_directories(nuclaw_step08 PRIVATE ${PQXX_INCLUDE_DIRS})
    target_compile_options(nuclaw_step08 PRIVATE ${PQXX_CFLAGS_OTHER})
    target_link_libraries(nuclaw_step08 ${PQXX_LIBRARIES})
    target_compile_definitions(nuclaw_step08 PRIVATE HAS_PQXX)
endif()

target_include_directories(nuclaw_step08 PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
