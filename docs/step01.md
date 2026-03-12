# Step 1: 异步 I/O 与 CMake 构建系统

> 目标：实现高并发，支持数千连接。引入 CMake 构建系统。
> 
> 代码量：约 120 行

## 本节收获

- 理解异步 I/O 模型（非阻塞、回调、事件驱动）
- 掌握 Session 模式管理连接
- 使用 CMake 管理 C++ 项目构建
- 实现多线程并发处理

## 为什么要异步？

### Step 0 的问题

```
同步模式（Step 0）：
客户端1 ──▶ [accept] ──▶ [read] ──▶ [write] ──▶ 关闭
                      ↑
客户端2 ──▶ [等待...]   [阻塞中，无法处理新连接]
```

**问题**：一个客户端占用服务器期间，其他客户端必须等待。

### 异步模式（Step 1）

```
异步模式：
客户端1 ──▶ [async_accept] ──▶ 回调处理 ──▶ 完成
           ↓
客户端2 ──▶ [立即接受] ──▶ 回调处理 ──▶ 完成
           ↓
客户端N ──▶ [同时处理数千连接]
```

**优势**：
- 不阻塞主线程
- 一个线程管理多个连接
- 多线程可以并行处理

## 核心概念

### 异步 I/O 三要素

```cpp
// 1. 异步操作
socket_.async_read_some(buffer, callback);

// 2. 回调函数（Lambda）
[capture](error_code ec, size_t len) {
    // 操作完成时执行
};

// 3. 事件循环
io_context.run();  // 分发事件到回调
```

### Session 模式

```cpp
// 每个连接一个 Session 对象
class Session : public std::enable_shared_from_this<Session> {
    tcp::socket socket_;
    // 管理一个连接的生命周期
};

// Session 自动管理内存（shared_ptr）
auto session = std::make_shared<Session>(std::move(socket));
session->start();  // 开始异步处理
```

## CMake 基础

### 为什么要用 CMake？

| 场景 | g++ 命令 | CMake |
|:---|:---|:---|
| 单文件 | `g++ main.cpp -o server` | 简单但没必要 |
| 多文件 | 命令很长 | 自动处理依赖 |
| 跨平台 | 不同命令 | 统一配置 |
| 找库 | 手动指定路径 | 自动查找 |

### CMake 工作流程

```
CMakeLists.txt ──▶ cmake ──▶ Makefile ──▶ make ──▶ 可执行文件
     (配置)          (生成)        (编译)
```

### 本项目的 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.14)  # 最低版本
project(nuclaw_step01)                 # 项目名称

set(CMAKE_CXX_STANDARD 17)             # C++17

# 查找依赖
find_package(Boost REQUIRED)

# 添加可执行文件
add_executable(nuclaw_step01 main.cpp)

# 链接库
target_link_libraries(nuclaw_step01 Boost::system)
```

## 编译运行

### 使用 CMake 构建

```bash
# 1. 创建构建目录（推荐 out-of-source 构建）
mkdir build && cd build

# 2. 生成构建文件
cmake ..

# 3. 编译
make -j$(nproc)  # 使用所有 CPU 核心

# 4. 运行
./nuclaw_step01
```

### 输出

```
NuClaw Step 1 - Async I/O
Listening on http://localhost:8080
```

### 测试

```bash
# 测试多个并发连接
for i in {1..10}; do
    curl http://localhost:8080/ &
done
wait
```

## 代码解析

### 1. 异步读取

```cpp
void do_read() {
    auto self = shared_from_this();  // 保持 Session 存活
    
    socket_.async_read_some(
        asio::buffer(buffer_),
        [this, self](boost::system::error_code ec, size_t len) {
            // 数据到达时回调
            if (!ec) {
                process_data(len);
                do_write(response);
            }
        }
    );
    // 立即返回，不阻塞！
}
```

### 2. 异步写入

```cpp
void do_write(const string& response) {
    auto self = shared_from_this();
    
    asio::async_write(socket_, asio::buffer(response),
        [this, self](error_code ec, size_t) {
            socket_.close();  // 写完后关闭
        }
    );
}
```

### 3. 多线程运行

```cpp
// 创建多个线程运行 io_context
vector<thread> threads;
for (unsigned i = 0; i < thread::hardware_concurrency(); ++i) {
    threads.emplace_back([&io]() { io.run(); });
}

// 等待所有线程完成
for (auto& t : threads) {
    t.join();
}
```

**效果**：
- 4 核 CPU → 4 个线程
- 每个线程处理部分连接
- 充分利用多核性能

## 与 Step 0 的对比

| 特性 | Step 0 | Step 1 |
|:---|:---|:---|
| I/O 模型 | 同步阻塞 | 异步非阻塞 |
| 并发能力 | 1 | 数千 |
| 代码复杂度 | 简单 | 中等 |
| 线程数 | 1 | 多线程 |
| 构建工具 | g++ | CMake |

## 常见问题

### Q: 编译报错 `Boost not found`

**A:** 安装 Boost 并指定路径：
```bash
cmake -DBoost_ROOT=/usr/local/boost ..
```

### Q: `shared_from_this()` 是什么？

**A:** 让对象自己管理生命周期。异步回调可能在对象销毁后执行，`shared_ptr` 确保对象存活。

### Q: 可以混合同步和异步吗？

**A:** 可以但不推荐。异步代码中调用同步操作会阻塞事件循环。

## 下一步

→ **Step 2: HTTP 长连接优化**

在 Step 2 中，你将学习：
- HTTP Keep-Alive 实现
- 连接复用
- 请求流水线

---

**本节代码:** [src/step01/main.cpp](../../src/step01/main.cpp)  
**CMake 配置:** [CMakeLists.txt](../../CMakeLists.txt)
