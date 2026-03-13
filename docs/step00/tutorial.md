# Step 0: 最简单的 HTTP Echo 服务器

> 目标：用 50 行代码理解 HTTP 服务器的核心概念
> 
> 难度：⭐ (入门)
> 
> 代码量：约 50 行

## 本节收获

- 理解阻塞式 I/O 的工作原理
- 掌握 Boost.Asio 的基本概念
- 学会构造简单的 HTTP 响应

---

## 前置知识

### 什么是 HTTP 服务器？

HTTP 服务器就是一个程序，它：
1. **监听**一个端口（如 8080）
2. **等待**客户端（浏览器、curl）连接
3. **接收**客户端发送的请求
4. **处理**请求并生成响应
5. **发送**响应给客户端
6. **关闭**连接（短连接模式）

### 同步 vs 异步

| 模式 | 特点 | 适用场景 |
|:---|:---|:---|
| **同步（阻塞）** | 代码简单，一次处理一个请求 | 学习、低并发 |
| **异步（非阻塞）** | 代码复杂，可同时处理多个请求 | 生产环境、高并发 |

**本节使用同步模式**，代码更易理解。

---

## 核心概念图解

```
┌─────────────┐                    ┌─────────────┐
│   客户端     │  ──── ①连接 ────▶  │   服务器     │
│  (浏览器)    │                    │   (8080)    │
│             │  ◀─── ②响应 ─────  │              │
└─────────────┘                    └─────────────┘
         
流程：
1. 服务器启动，监听 8080 端口
2. 客户端发起 TCP 连接
3. 服务器接受连接
4. 服务器读取请求
5. 服务器发送响应
6. 连接关闭
```

---

## 代码逐行解析

### 1. 包含头文件

```cpp
#include <boost/asio.hpp>    // Asio 核心库
#include <iostream>           // 输入输出
#include <string>             // 字符串处理
```

**Boost.Asio** 是 C++ 最流行的网络库，提供了跨平台的 I/O 功能。

### 2. 简化命名空间

```cpp
using boost::asio::ip::tcp;
```

这样可以直接写 `tcp::acceptor` 而不是 `boost::asio::ip::tcp::acceptor`。

### 3. 构造 HTTP 响应

```cpp
std::string make_response(const std::string& body) {
    return "HTTP/1.1 200 OK\r\n"                           // 状态行
           "Content-Type: application/json\r\n"            // 响应头
           "Content-Length: " + std::to_string(body.length()) + "\r\n"  // 内容长度
           "\r\n" + body;                                   // 空行 + 响应体
}
```

HTTP 响应格式：
```
HTTP/1.1 200 OK\r\n           ← 状态行（协议 状态码 状态描述）
Content-Type: json\r\n        ← 响应头（告诉客户端内容类型）
Content-Length: 123\r\n       ← 响应头（告诉客户端内容长度）
\r\n                          ← 空行（表示头部结束）
{...}                         ← 响应体（实际内容）
```

### 4. io_context - Asio 的心脏

```cpp
boost::asio::io_context io;
```

`io_context` 是 Asio 的核心类：
- 管理所有的 I/O 操作
- 在同步模式下，它协调阻塞调用
- 在异步模式下（后面步骤），它是事件循环的核心

### 5. acceptor - 监听器

```cpp
tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 8080));
```

- `tcp::endpoint(tcp::v4(), 8080)`：绑定到 IPv4 的 8080 端口
- `acceptor`：专门用于**接受**新连接的对象

### 6. socket - 连接的代表

```cpp
tcp::socket socket(io);
acceptor.accept(socket);
```

- `socket` 代表一个 TCP 连接
- `accept()` 是**阻塞**的：程序会停在这里，直到有客户端连接

### 7. 读取请求

```cpp
char buffer[1024] = {};
size_t len = socket.read_some(boost::asio::buffer(buffer));
```

- `buffer`：用于存储接收到的数据
- `read_some()`：读取数据，返回实际读取的字节数
- 这也是**阻塞**的：如果没有数据，程序会等待

### 8. 发送响应

```cpp
boost::asio::write(socket, boost::asio::buffer(response));
```

将响应数据发送给客户端。

### 9. 连接关闭

```cpp
// socket 在这里析构，连接自动关闭
```

当 `socket` 变量离开作用域，析构函数会自动关闭连接。

---

## 完整编译运行

### 1. 安装依赖（Ubuntu/Debian）

```bash
sudo apt-get update
sudo apt-get install -y build-essential libboost-all-dev
```

### 2. 编译

```bash
cd src/step00
g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
```

参数说明：
- `-std=c++17`：使用 C++17 标准
- `-o server`：输出文件名为 server
- `-lboost_system`：链接 Boost.System 库
- `-lpthread`：链接线程库（Asio 需要）

### 3. 运行

```bash
./server
```

你应该看到：
```
========================================
  NuClaw Step 0 - HTTP Echo Server
========================================
Server listening on http://localhost:8080
Press Ctrl+C to stop
```

### 4. 测试

打开另一个终端：

```bash
curl http://localhost:8080
```

输出：
```json
{
    "status": "ok",
    "step": 0,
    "message": "Hello from NuClaw!",
    "note": "This is the simplest HTTP server"
}
```

或者用浏览器访问 `http://localhost:8080`。

---

## 常见问题

### Q: 编译报错 `boost/asio.hpp: No such file`

A: 安装 Boost 开发包：
```bash
sudo apt-get install libboost-all-dev
```

### Q: 端口被占用

A: 更换端口或杀死占用进程：
```bash
# 查看占用 8080 的进程
sudo lsof -i :8080

# 杀死进程（将 <PID> 替换为实际的进程号）
kill -9 <PID>
```

### Q: 为什么每次请求后连接都关闭？

A: 这是**短连接**模式。socket 在每次循环结束时析构，连接关闭。Step 2 会实现 Keep-Alive 长连接。

---

## 下一步

→ **Step 1: 异步 I/O 与 CMake**

我们将学习：
- 异步编程模型
- 使用 CMake 构建项目
- Session 设计模式
