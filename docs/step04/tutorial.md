# Step 4: Agent Loop - 执行引擎

> 目标：实现循环检测、并行工具执行
> 
> 代码量：约 320 行

## 本节收获

- 工具调用循环检测
- 并行工具执行
- 执行引擎架构

## 核心特性

### 1. 循环检测

检测连续相同的工具调用，防止无限循环：

```cpp
bool detect_loop(Session& session) {
    if (session.recent_tool_calls.size() < 3) return false;
    // 检查最后3次是否相同
    auto& calls = session.recent_tool_calls;
    size_t n = calls.size();
    return calls[n-1] == calls[n-2] && calls[n-2] == calls[n-3];
}
```

### 2. 并行执行

使用 std::async 并行执行多个工具：

```cpp
std::vector<ToolResult> execute_tools_parallel(
    const std::vector<ToolCall>& calls) {
    std::vector<std::future<ToolResult>> futures;
    for (const auto& call : calls) {
        futures.push_back(std::async(std::launch::async, 
            [&call]() { return execute_tool(call); }));
    }
    // 收集结果...
}
```

## 编译运行

```bash
cd src/step04
mkdir build && cd build
cmake .. && make
./nuclaw_step04
```
