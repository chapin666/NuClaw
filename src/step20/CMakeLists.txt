cmake_minimum_required(VERSION 3.14)
project(smartsupport VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPQXX REQUIRED libpqxx)

# 第三方库（FetchContent）
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(json)

# 公共库
add_library(common STATIC
    src/common/types.cpp
    src/common/utils.cpp
    src/common/database.cpp
)
target_link_libraries(common
    Boost::system
    Threads::Threads
    ${LIBPQXX_LIBRARIES}
    nlohmann_json::nlohmann_json
)
target_include_directories(common PUBLIC include)

# 服务库
add_library(chat_service STATIC
    src/services/chat/service.cpp
    src/services/chat/session.cpp
)
target_link_libraries(chat_service common)

add_library(ai_service STATIC
    src/services/ai/service.cpp
    src/services/ai/agent.cpp
    src/services/ai/rag.cpp
)
target_link_libraries(ai_service common)

add_library(knowledge_service STATIC
    src/services/knowledge/service.cpp
    src/services/knowledge/indexer.cpp
    src/services/knowledge/retriever.cpp
)
target_link_libraries(knowledge_service common)

# 可执行文件
add_executable(smartsupport src/main.cpp)
target_link_libraries(smartsupport
    chat_service
    ai_service
    knowledge_service
    common
)