cmake_minimum_required(VERSION 3.14)

# 下载 Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

# 测试可执行文件
add_executable(nuclaw_tests
    test_main.cpp
    test_tool_registry.cpp
    test_vector_store.cpp
)

target_link_libraries(nuclaw_tests
    gtest_main
    ${Boost_LIBRARIES}
    Threads::Threads
)

# 添加测试
add_test(NAME nuclaw_unit_tests COMMAND nuclaw_tests)
