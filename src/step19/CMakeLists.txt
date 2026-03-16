# ============================================================================
# CMakeLists.txt - Step 18: 核心服务拆分
# ============================================================================
# 演进说明：
#   从 Step 17 演进，保持相同结构
#   新增 services/ 目录的头文件
# ============================================================================

cmake_minimum_required(VERSION 3.14)
project(nuclaw-step18 VERSION 1.0.0 LANGUAGES CXX)

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

# 可执行文件
add_executable(step18_demo src/main.cpp)

target_link_libraries(step18_demo
    Boost::system
    Boost::thread
    Boost::json
    Threads::Threads
    nlohmann_json::nlohmann_json
)

# 安装目标
install(TARGETS step18_demo DESTINATION bin)

# 打印配置信息
message(STATUS "========================================")
message(STATUS "NuClaw Step 18 Configuration")
message(STATUS "========================================")
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Boost Version: ${Boost_VERSION}")
message(STATUS "========================================")
message(STATUS "演进：Step 17 → Step 18")
message(STATUS "  - 拆分 CustomerServiceAgent 为 3 个服务")
message(STATUS "  - ChatService: 会话管理")
message(STATUS "  - AIService: 智能回复")
message(STATUS "  - KnowledgeService: 知识库/RAG")
message(STATUS "========================================")
