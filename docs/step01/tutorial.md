# Step 1: 异步 I/O 与 CMake 构建系统

> 目标：掌握异步编程模型，使用现代 CMake 构建项目
> 
> 难度：⭐⭐ (进阶)
> 
> 代码量：约 120 行

## 本节收获

- 理解异步 I/O 的工作原理
- 掌握 `shared_ptr` 和 `enable_shared_from_this`
- 学会使用 CMake 构建 C++ 项目
- 理解多线程 io_context

---

## 为什么需要异步？

### Step 0 的问题

Step 0 使用**同步阻塞**模式：

```
┌─────────┐     ┌─────────┐     ┌─────────┐
│ accept  │────▶│  read   │────▶│  write  │
│(阻塞等待)│     │(阻塞等待)│     │(阻塞等待)│
└─────────┘     └─────────┘     └─────────┘
```

一个请求处理时，服务器**无法**接受新连接。

### 异步解决方案

```
        ┌─→ [处理连接A] ─┐
        │                │
主线程 ─┼─→ [处理连接B] ─┼→ 所有操作都是非阻塞的
        │                │
        └─→ [处理连接C] ─┘
```

使用异步 I/O，一个线程可以同时处理多个连接。

---

## 核心概念图解

```
┌─────────────────────────────────────────────────────────┐
│                    异步服务器架构                          │
├─────────────────────────────────────────────────────────┤
│                                                         │
│   ┌─────────────┐                                       │
│   │  io_context │ ◄── 事件循环核心                        │
│   └──────┬──────┘                                       │
│          │                                              │
│    ┌─────┴─────┐                                        │
│    ▼           ▼                                        │
│ ┌────────┐  ┌────────┐                                  │
│ │Acceptor│  │SessionA│                                  │
│ └────┬───┘  └────┬───┘                                  │
│      │           │                                      │
│      │     ┌─────┴─────┐                                │
│      │     ▼           ▼                                │
│      │  ┌──────┐   ┌──────┐                             │
│      │  │Read  │   │Write │                             │
│      │  └──────┘   └──────┘                             │
│      │                                                  │
│      └──────────────┐                                   │
│                     ▼                                   │
│               新连接时创建                                 │
│               ┌────────┐                                │
│               │SessionB│                                │
│               └────────┘                                │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

---

## 新组件介绍

### 1. HTTP 请求解析器

```cpp
struct HttpRequest {
    std::string method;   // GET, POST, etc.
    std::string path;     // /api/user
    std::string body;     // 请求体
    std::map<std::string, std::string> headers;
};
```

HTTP 请求格式：
```
GET /api/user HTTP/1.1\r\n          ← 请求行
Host: localhost:8080\r\n            ← 请求头
Content-Type: application/json\r\n  ← 请求头
\r\n                                 ← 空行
{"name":"test"}                    ← 请求体（可选）
```

### 2. 路由系统 (Router)

```cpp
class Router {
    void add(const std::string& path, Handler handler);
    std::string handle(const HttpRequest& req);
};
```

路由系统负责：
- 注册 URL 处理函数
- 根据请求路径分发到对应处理器

### 3. Session 类

```cpp
class Session : public std::enable_shared_from_this<Session> {
    void start();      // 开始处理连接
    void do_read();    // 异步读取
    void do_write();   // 异步写入
};
```

Session 代表一个客户端连接的生命周期。

---

## 关键技术详解

### 1. shared_ptr 和 enable_shared_from_this

```cpp
class Session : public std::enable_shared_from_this<Session> {
    void do_read() {
        auto self = shared_from_this();  // 获取智能指针
        socket_.async_read_some(..., 
            [this, self](...) {  // 捕获 self 保持对象存活
                // 回调执行时，Session 对象仍然存在
            }
        );
    }
};
```

**为什么要用 shared_ptr？**

异步回调可能在很久以后执行。如果 Session 是普通对象，回调执行时对象可能已经销毁，导致**悬垂指针**。

`shared_ptr` 通过引用计数保证：只要有回调持有 `self`，Session 就不会被销毁。

**图解：**
```
创建 Session ──▶ shared_ptr (ref=1)
                    │
    async_read ────┼──▶ 回调捕获 self (ref=2)
                    │
    回调执行完成 ◀──┘ (ref=1)
                    │
    连接关闭 ◀────── (ref=0, 自动销毁)
```

### 2. 异步读取

```cpp
void do_read() {
    auto self = shared_from_this();
    socket_.async_read_some(
        asio::buffer(buffer_),
        [this, self](boost::system::error_code ec, std::size_t len) {
            // 回调函数：数据到达时执行
            if (!ec) {
                // 处理数据
                do_write(response);
            }
        }
    );
    // 函数立即返回，不阻塞！
}
```

**关键点：**
- `async_read_some` 立即返回
- 当数据到达时，io_context 调用回调函数
- 回调在 `io.run()` 的线程中执行

### 3. 多线程 io_context

```cpp
std::vector<std::thread> threads;
auto count = std::thread::hardware_concurrency();
for (unsigned i = 0; i < count; i++) {
    threads.emplace_back([&io]() { io.run(); });
}
```

`io.run()` 启动事件循环：
- 等待异步操作完成
- 调用对应的回调函数
- 重复直到 io_context 停止

多线程运行 `io.run()` 可以充分利用多核 CPU。

---

## CMake 构建系统

### 什么是 CMake？

CMake 是一个**跨平台**的构建工具：
- 写一份 `CMakeLists.txt`
- 生成 Visual Studio、Makefile、Ninja 等各种项目文件
- 自动处理依赖、编译选项等

### CMakeLists.txt 解析

```cmake
cmake_minimum_required(VERSION 3.14)     # 最低 CMake 版本
project(nuclaw_step01 LANGUAGES CXX)      # 项目名称

set(CMAKE_CXX_STANDARD 17)                # C++17 标准
set(CMAKE_CXX_STANDARD_REQUIRED ON)       # 强制要求

# 查找 Boost 库
find_package(Boost 1.70 REQUIRED COMPONENTS system thread)

# 添加可执行文件
add_executable(nuclaw_step01 main.cpp)

# 链接库
target_link_libraries(nuclaw_step01 
    Boost::system 
    Boost::thread
)
```

### 构建步骤

```bash
# 1. 创建构建目录（推荐 out-of-source 构建）
mkdir build && cd build

# 2. 生成构建文件
cmake ..

# 3. 编译（-j 并行编译）
make -j$(nproc)
```

**为什么要用 build 目录？**
- 保持源码目录干净
- 可以创建多个构建配置（Debug/Release）

---

## 完整运行测试

### 1. 编译运行

```bash
cd src/step01
mkdir build && cd build
cmake .. && make -j4
./nuclaw_step01
```

### 2. 测试路由

```bash
# 测试根路由
curl http://localhost:8080/
# 输出: {"status":"ok","step":1,"features":"async+multithread"}

# 测试健康检查
curl http://localhost:8080/health
# 输出: {"health":"ok","threads":8}

# 测试 404
curl http://localhost:8080/notexist
# 输出: {"error":"not found"}
```

### 3. 并发测试

```bash
# 使用 ab (Apache Bench) 测试并发
ab -n 10000 -c 100 http://localhost:8080/
```

---

## 与 Step 0 的对比

| 特性 | Step 0 (同步) | Step 1 (异步) |
|:---|:---|:---|
| 代码复杂度 | 简单 | 较复杂 |
| 并发能力 | 单连接 | 多连接 |
| 资源占用 | 低 | 较低 |
| 适用场景 | 学习/测试 | 生产环境 |
| 构建工具 | g++ | CMake |

---

## 常见问题

### Q: `shared_from_this()` 报错 "bad_weak_ptr"

A: 必须先用 `shared_ptr` 管理对象：
```cpp
// 正确
auto session = std::make_shared<Session>(...);
session->start();

// 错误
Session session(...);
session.start();  // shared_from_this() 会崩溃
```

### Q: CMake 找不到 Boost

A: 指定 Boost 路径：
```bash
cmake -DBOOST_ROOT=/usr/local/boost ..
```

### Q: 程序崩溃，提示 "double free"

A: 检查是否正确使用了 `enable_shared_from_this` 和 `shared_ptr`。

---

## 下一步

→ **Step 2: HTTP 长连接优化**

我们将学习：
- HTTP Keep-Alive 机制
- 连接复用
- 超时管理
