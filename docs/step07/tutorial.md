# Step 7: Tools 系统（中）- 异步工具执行

> 目标：实现异步工具执行，解决同步工具的阻塞问题
> 
> 代码量：约 560 行

## 本节收获

- 理解异步工具执行的必要性
- 实现工具级别的并发控制
- 掌握异步回调模式
- 理解任务队列和背压

---

## 为什么需要异步工具？

### 同步工具的问题

```cpp
// 同步执行 HTTP 请求
auto result = http_tool.execute(args);  // 阻塞 2 秒！
// 这 2 秒内，Agent 无法处理其他请求
```

**影响：**
- 单个慢工具拖垮整个系统
- 无法并行执行多个工具
- 用户体验差（响应慢）

### 异步解决方案

```cpp
// 异步执行
http_tool.execute_async(args, [](json::value result) {
    // 回调：结果返回时执行
});
// 立即返回，不阻塞！
```

---

## 核心设计

### Tool 接口升级

```cpp
class Tool {
public:
    // 纯虚函数：子类必须实现异步版本
    virtual void execute_async(
        const json::object& args,
        std::function<void(json::value)> callback
    ) = 0;
    
    // 基类提供同步包装
    json::value execute_sync(const json::object& args) {
        std::promise<json::value> promise;
        execute_async(args, [&promise](auto result) {
            promise.set_value(result);
        });
        return promise.get_future().get();
    }
};
```

### 并发控制

```cpp
class ToolExecutor {
    // 限制同时执行的工具数量
    // 防止资源耗尽
    
    std::queue<Task> queue_;      // 等待队列
    size_t max_concurrent_;         // 最大并发数
    size_t running_;                // 当前运行数
};
```

### 超时控制

```cpp
void execute_with_timeout(
    std::shared_ptr<Tool> tool,
    std::chrono::milliseconds timeout,
    Callback callback
) {
    // 1. 启动工具
    // 2. 启动定时器
    // 3. 先到先回调，取消另一个
}
```

---

## 运行测试

```bash
cd src/step07
mkdir build && cd build
cmake .. && make
./nuclaw_step07

# 测试
wscat -c ws://localhost:8081
> {\"tool\": \"http_request\", \"args\": {\"url\": \"https://api.example.com\"}}
```

---

## 下一步

→ **Step 8: Tools 系统（下）- 工具生态**

实际工具实现：HTTP 客户端、文件操作、代码执行沙箱。
