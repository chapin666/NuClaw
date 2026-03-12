# Step 2: 异步 I/O 和多线程

> 目标：实现高并发，支持数千连接。
> 
> 代码量：约 150 行

## 核心改进

- **异步 I/O**: `async_accept`, `async_read`, `async_write`
- **Session 模式**: 每个连接一个 Session 对象
- **多线程**: 多个线程共享 `io_context`

## 架构对比

```
同步模式 (Step 1)         异步模式 (Step 2)
     │                          │
     ▼                          ▼
┌─────────┐              ┌─────────────┐
│ accept  │              │ async_accept│◄────┐
│ 阻塞     │              │  非阻塞      │     │
└────┬────┘              └──────┬──────┘     │
     │                          │            │
     ▼                          ▼            │
┌─────────┐              ┌─────────────┐    │
│  read   │              │ async_read  │────┘
│ 阻塞     │              │ 回调处理     │
└─────────┘              └─────────────┘
```

## 关键代码

```cpp
// 异步读取
socket_.async_read_some(buffer_,
    [this](error_code ec, size_t len) {
        if (!ec) process_data(len);
    }
);
```

## 性能

| 指标 | 同步 | 异步 |
|------|------|------|
| 并发 | 1 | 数千 |
| 阻塞 | 是 | 否 |
| CPU | 单核 | 多核 |

## 编译

```bash
g++ -std=c++17 main.cpp -o server \
    -lboost_system -lboost_thread -lpthread
```

## 下一步

→ [Step 3: WebSocket](step03.md)
