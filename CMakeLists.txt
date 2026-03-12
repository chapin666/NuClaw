# Step 1 CMakeLists.txt
cmake_minimum_required(VERSION 3.14)
project(nuclaw_step01 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 Boost
find_package(Boost 1.70 REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)

# 添加可执行文件
add_executable(nuclaw_step01 main.cpp)

# 链接库
target_link_libraries(nuclaw_step01 
    Boost::system 
    Boost::thread
    Threads::Threads
)

# 安装目标
install(TARGETS nuclaw_step01 RUNTIME DESTINATION bin)
