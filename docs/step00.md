# Step 0: 最简单的 HTTP Echo 服务器

> 目标：创建一个最基本的 HTTP 服务器，能响应请求。
> 
> 代码量：约 50 行

## 本节收获

- 理解 Boost.Asio 的核心概念（`io_context`、`acceptor`、`socket`）
- 掌握同步 TCP 服务器的编写
- 了解 HTTP 协议的最简实现

## 架构图

```
┌─────────────────────────────────────────────────────────────┐
│                    Step 0 同步服务器架构                      │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   ┌──────────────┐                                          │
│   │   客户端      │                                          │
│   │  (curl/浏览器)│                                          │
│   └──────┬───────┘                                          │
│          │ HTTP 请求                                         │
│          ▼                                                  │
│   ┌──────────────┐     ┌──────────────┐                    │
│   │   acceptor   │◄────┤ io_context   │                    │
│   │   (监听8080)  │     │   (事件循环)  │                    │
│   └──────┬───────┘     └──────────────┘                    │
│          │ accept() 阻塞等待                                  │
│          ▼                                                  │
│   ┌──────────────┐                                          │
│   │    socket    │◄──────── 建立 TCP 连接                    │
│   │  (连接代表)   │                                          │
│   └──────┬───────┘                                          │
│          │ read_some() 阻塞读取                               │
│          ▼                                                  │
│   ┌──────────────┐                                          │
│   │  处理请求     │  构造 HTTP 响应                            │
│   │  (echo逻辑)  │                                          │
│   └──────┬───────┘                                          │
│          │ write() 发送响应                                   │
│          ▼                                                  │
│   ┌──────────────┐                                          │
│   │   关闭连接    │◄──────── 短连接，每次请求后关闭            │
│   └──────────────┘                                          │
│                                                             │
│   【特点】同步阻塞，一次只能处理一个请求                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## 核心概念

### Boost.Asio 基础组件

```cpp
asio::io_context io;                    // 事件循环，管理所有 I/O
tcp::acceptor acceptor(io, endpoint);   // 监听端口，接受连接
tcp::socket socket(io);                 // 代表一个 TCP 连接
acceptor.accept(socket);                // 阻塞等待客户端连接
```

### HTTP 协议简化

HTTP/1.1 最简单的响应格式：

```
HTTP/1.1 200 OK\r\n              ← 状态行
Content-Type: application/json\r\n  ← 响应头
Content-Length: 27\r\n              ← 响应头
\r\n                                ← 空行（分隔头部和 Body）
{"status":"ok"}                   ← 响应体
```

## 程序流程图

```
┌─────────┐
│  main() │
└────┬────┘
     │
     ▼
┌─────────────────┐
│ 创建 io_context  │
└────────┬────────┘
         │
         ▼
┌─────────────────┐     否      ┌─────────┐
│ 创建 acceptor    │────────────►│  退出   │
│ 监听 8080 端口   │             └─────────┘
└────────┬────────┘
         │ 是
         ▼
┌─────────────────┐
│   while(true)   │◄──────────────────────────┐
└────────┬────────┘                           │
         │                                    │
         ▼                                    │
┌─────────────────┐     阻塞等待              │
│ accept(socket)  │◄─────────────────────┐   │
│  等待客户端连接  │                      │   │
└────────┬────────┘                      │   │
         │ 连接建立                       │   │
         ▼                               │   │
┌─────────────────┐                      │   │
│ read_some()     │  阻塞读取请求         │   │
│ 读取 HTTP 请求   │                      │   │
└────────┬────────┘                      │   │
         │                               │   │
         ▼                               │   │
┌─────────────────┐                      │   │
│ 构造 JSON 响应   │                      │   │
│ {"status":"ok"} │                      │   │
└────────┬────────┘                      │   │
         │                               │   │
         ▼                               │   │
┌─────────────────┐                      │   │
│ write(response) │  发送响应             │   │
│ 发送 HTTP 响应   │                      │   │
└────────┬────────┘                      │   │
         │                               │   │
         ▼                               │   │
┌─────────────────┐                      │   │
│  socket 析构    │  关闭连接             │   │
│  (短连接模式)   │                      │   │
└────────┬────────┘                      │   │
         │                               │   │
         └───────────────────────────────┘   │
                                              │
         ┌────────────────────────────────────┘
         │
         ▼
┌─────────────────┐
│   下一个循环     │  回到 while(true)
└─────────────────┘
```

## 编译运行

### 安装依赖

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y libboost-all-dev
```

**macOS:**
```bash
brew install boost
```

**CentOS/RHEL:**
```bash
sudo yum install boost-devel
```

### 编译

使用 g++ 直接编译（**CMake 将在 Step 1 中介绍**）：

```bash
cd src/step00
g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
```

参数说明：
- `-std=c++17`: 使用 C++17 标准
- `-lboost_system`: 链接 Boost.System 库
- `-lpthread`: 链接 POSIX 线程库（Asio 需要）

### 运行

```bash
./server
```

输出：
```
========================================
  NuClaw Step 0 - HTTP Echo Server
========================================
Server listening on http://localhost:8080
Press Ctrl+C to stop
```

### 测试

```bash
# 使用 curl 测试
curl http://localhost:8080

# 输出：
# {
#     "status": "ok",
#     "step": 0,
#     "message": "Hello from NuClaw!",
#     "note": "This is the simplest HTTP server"
# }
```

或者用浏览器访问 `http://localhost:8080`。

## 代码解析

### 1. 头文件

```cpp
#include <boost/asio.hpp>    // Boost.Asio 核心库
#include <iostream>           // 标准输入输出
#include <string>             // 字符串处理
```

### 2. 构造 HTTP 响应

```cpp
std::string make_response(const std::string& body) {
    return "HTTP/1.1 200 OK\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: " + std::to_string(body.length()) + "\r\n"
           "\r\n" + body;
}
```

**关键点：**
- `\r\n` 是 HTTP 规定的行结束符（回车+换行）
- `Content-Length` 必须准确，否则客户端无法判断响应结束
- 空行 `\r\n` 分隔头部和 Body，必不可少

### 3. 主函数流程

```cpp
int main() {
    // 1. 创建 I/O 上下文
    boost::asio::io_context io;
    
    // 2. 创建 acceptor，监听 8080 端口
    tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 8080));
    
    // 3. 主循环：持续接受连接
    while (true) {
        tcp::socket socket(io);      // 创建 socket
        acceptor.accept(socket);      // 阻塞等待连接
        
        // 4. 读取请求
        char buffer[1024] = {};
        socket.read_some(boost::asio::buffer(buffer));
        
        // 5. 发送响应
        auto response = make_response(body);
        boost::asio::write(socket, boost::asio::buffer(response));
        
        // 6. socket 析构，连接关闭（短连接）
    }
}
```

## 设计决策分析

### 为什么用同步 I/O？

**优点：**
- 代码简单直观，易于理解
- 适合学习原型

**缺点：**
- 阻塞式，一次只能处理一个请求
- 无法利用多核 CPU

**改进方向：**
- Step 1 将引入异步 I/O 和线程池

### 为什么是短连接？

每个请求处理完就关闭连接：
- ✅ 简单，无需管理连接状态
- ❌ 效率低，每个请求都要建立/断开 TCP 连接

**改进方向：**
- Step 2 将引入 HTTP Keep-Alive 长连接

## 常见问题

### Q1: 编译报错 `boost/asio.hpp: No such file`

**A:** 安装 Boost 库：
```bash
sudo apt-get install libboost-all-dev  # Ubuntu/Debian
brew install boost                      # macOS
sudo yum install boost-devel            # CentOS/RHEL
```

### Q2: 运行后浏览器访问没响应

**A:** 检查：
1. 程序是否正常启动（看到 `Server listening` 消息）
2. 端口 8080 是否被占用：`lsof -i :8080`
3. 防火墙是否允许：`sudo ufw allow 8080`

### Q3: 如何停止服务器？

**A:** 按 `Ctrl+C` 发送 SIGINT 信号，程序会退出。

## 延伸思考

1. **如果同时有 100 个客户端请求，这个服务器会怎样？**
   - 第1个请求处理时，其他99个在排队等待
   - Step 1 的异步 I/O 将解决这个问题

2. **如何获取客户端请求的 Path 和 Headers？**
   - 需要解析 HTTP 请求字符串
   - Step 1 将实现完整的 HTTP 请求解析

3. **如何添加路由（不同 Path 返回不同内容）？**
   - 需要路由分发器
   - Step 1 将实现 Router 类

## 下一步

→ [Step 1: 异步 I/O 与 CMake 构建系统](../step01.md)

在 Step 1 中，你将学习：
- 异步 I/O 模型（`async_accept`、`async_read`）
- Session 模式管理连接
- CMake 构建系统

---

**本节代码:** [src/step00/main.cpp](../../src/step00/main.cpp)
