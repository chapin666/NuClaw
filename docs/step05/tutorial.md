# Step 5: Agent Loop - 高级特性

> 目标：实现记忆系统、上下文压缩、质量门控
> 
> 代码量：约 400 行

## 本节收获

- 长期记忆系统
- 上下文窗口管理
- 响应质量评估

## 核心特性

### 1. 记忆系统

```cpp
struct MemoryEntry {
    std::string key;
    std::string value;
    float importance = 1.0;
};

// 存储重要信息到长期记忆
void store_memory(const std::string& key, 
                  const std::string& value);
```

### 2. 上下文压缩

当 token 数超过限制时，自动压缩历史：

```cpp
void compress_context_if_needed() {
    if (current_tokens > max_context_tokens * 0.8) {
        // 生成摘要，替换详细历史
        summarize_history();
    }
}
```

### 3. 质量门控

```cpp
bool evaluate_quality(const std::string& response) {
    // 检查响应是否有效
    // 必要时重新生成
}
```

## 编译运行

```bash
cd src/step05
mkdir build && cd build
cmake .. && make
./nuclaw_step05
```
