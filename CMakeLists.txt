cmake_minimum_required(VERSION 3.14)
project(NuClaw LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 查找依赖
find_package(Boost 1.70 REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)

# 查找可选依赖
find_package(OpenSSL QUIET)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PQXX libpqxx)
endif()

# Step 0: Basic HTTP Echo
add_executable(nuclaw_step00 src/step00/main.cpp)
target_link_libraries(nuclaw_step00 
    Boost::system 
    Threads::Threads
)

# Step 1: HTTP Routing
add_executable(nuclaw_step01 src/step01/main.cpp)
target_link_libraries(nuclaw_step01 
    Boost::system 
    Boost::thread
    Threads::Threads
)

# Step 2: Async I/O
add_executable(nuclaw_step02 src/step02/main.cpp)
target_link_libraries(nuclaw_step02 
    Boost::system 
    Boost::thread
    Threads::Threads
)

# Step 3: WebSocket + Agent Loop
add_executable(nuclaw_step03 src/step03/main.cpp)
target_link_libraries(nuclaw_step03 
    Boost::system 
    Threads::Threads
)

# Step 4: LLM Integration (需要 OpenSSL)
if(OpenSSL_FOUND)
    add_executable(nuclaw_step04 src/step04/main.cpp)
    target_link_libraries(nuclaw_step04 
        Boost::system 
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
    )
else()
    message(WARNING "OpenSSL not found, nuclaw_step04 will not be built")
endif()

# Step 5: Multi-Tenant (需要 libpqxx)
if(PQXX_FOUND)
    add_executable(nuclaw_step05 src/step05/main.cpp)
    target_include_directories(nuclaw_step05 PRIVATE ${PQXX_INCLUDE_DIRS})
    target_link_libraries(nuclaw_step05 
        Boost::system 
        ${PQXX_LIBRARIES}
        Threads::Threads
    )
else()
    message(WARNING "libpqxx not found, nuclaw_step05 will not be built")
endif()

# 安装
install(TARGETS nuclaw_step00 nuclaw_step01 nuclaw_step02 nuclaw_step03
    RUNTIME DESTINATION bin
)

if(OpenSSL_FOUND)
    install(TARGETS nuclaw_step04 RUNTIME DESTINATION bin)
endif()

if(PQXX_FOUND)
    install(TARGETS nuclaw_step05 RUNTIME DESTINATION bin)
endif()

# 打印配置摘要
message(STATUS "")
message(STATUS "NuClaw Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Boost: ${Boost_VERSION}")
message(STATUS "  OpenSSL: ${OpenSSL_FOUND}")
message(STATUS "  libpqxx: ${PQXX_FOUND}")
message(STATUS "")
