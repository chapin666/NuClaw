# ============================================================================
# CMakeLists.txt - Step 17: 多租户智能客服平台
# ============================================================================
# 演进说明：
#   从 Step 16 的 CMakeLists.txt 演进
#   保持简单结构（头文件库），为 Step 18 服务拆分铺垫
# ============================================================================

cmake_minimum_required(VERSION 3.14)
project(nuclaw-step17 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_package(Boost REQUIRED COMPONENTS system thread json)
find_package(Threads REQUIRED)

# 第三方库（FetchContent）
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(json)

# 头文件搜索路径
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
)

# 可执行文件：step17_demo
# 演进说明：从 step16_demo 演进，添加多租户演示
add_executable(step17_demo
    src/main.cpp
)

target_link_libraries(step17_demo
    Boost::system
    Boost::thread
    Boost::json
    Threads::Threads
    nlohmann_json::nlohmann_json
)

# 安装目标
install(TARGETS step17_demo DESTINATION bin)

# 打印配置信息
message(STATUS "========================================")
message(STATUS "NuClaw Step 17 Configuration")
message(STATUS "========================================")
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Boost Version: ${Boost_VERSION}")
message(STATUS "========================================")
message(STATUS "演进：Step 16 → Step 17")
message(STATUS "  - 复用 include/nuclaw/*.hpp (Step 16)")
message(STATUS "  - 新增 tenant.hpp, customer_service_agent.hpp")
message(STATUS "  - 修改 main.cpp 展示多租户")
message(STATUS "========================================")
