# Step 0: 最简单的 HTTP Echo 服务器

> 目标：用 50 行代码理解 HTTP 服务器的核心概念
> 
> 难度：⭐ (入门)
> 
> 代码量：约 50 行

## 本节收获

- 理解计算机网络基础（TCP/IP、端口、Socket）
- 掌握阻塞式 I/O 的工作原理
- 学会使用 Boost.Asio 构建网络应用
- 理解 HTTP 协议的文本格式

---

## 第一部分：网络编程基础

### 1.1 TCP/IP 协议栈

互联网通信依赖分层协议：

```
┌─────────────────────────────────────────┐
│  应用层  │  HTTP / FTP / SSH / WebSocket  │ ← 我们编写代码的层级
├─────────────────────────────────────────┤
│  传输层  │  TCP / UDP                     │ ← 提供可靠/不可靠传输
├─────────────────────────────────────────┤
│  网络层  │  IP / ICMP                     │ ← 寻址和路由
├─────────────────────────────────────────┤
│ 链路层  │  Ethernet / WiFi               │ ← 物理传输
└─────────────────────────────────────────┘
```

**HTTP 服务器工作在应用层**，基于 TCP 协议传输数据。

### 1.2 什么是 Socket？

Socket（套接字）是操作系统提供的**网络通信接口**：

```
┌──────────┐         网络          ┌──────────┐
│  程序 A   │ ◀───── Socket ─────▶ │  程序 B   │
│ (浏览器)  │      (操作系统)       │ (服务器)  │
└──────────┘                      └──────────┘
```

Socket 让程序像读写文件一样进行网络通信：
- `socket.read()` 接收数据
- `socket.write()` 发送数据

### 1.3 IP 地址与端口

**IP 地址**：定位网络中的一台设备
- `127.0.0.1`：本机（localhost）
- `192.168.x.x`：局域网地址
- 公网 IP：全球唯一

**端口**：定位设备上的一个程序
- `80`：HTTP 默认端口
- `443`：HTTPS 默认端口
- `8080`：开发常用端口
- 范围：0-65535（0-1023 需要管理员权限）

```
完整地址 = IP:端口
例如：127.0.0.1:8080
```

### 1.4 客户端-服务器模型

```
┌──────────────────────────────────────────────────────┐
│                    通信流程                           │
├──────────────────────────────────────────────────────┤
│                                                      │
│   服务器                    客户端                    │
│     │                         │                      │
│     │ ① 创建 Socket           │                      │
│     │    socket()             │                      │
│     │                         │                      │
│     │ ② 绑定地址和端口        │                      │
│     │    bind("0.0.0.0:8080") │                      │
│     │                         │                      │
│     │ ③ 开始监听              │                      │
│     │    listen()             │                      │
│     │         │               │                      │
│     │◀────────┘               │                      │
│     │                         │ ④ 发起连接           │
│     │◀────────────────────────│    connect()         │
│     │                         │                      │
│     │ ⑤ 接受连接              │                      │
│     │    accept() ────────────▶ 建立连接              │
│     │                         │                      │
│     │◀────────────────────────│ ⑥ 发送请求           │
│     │ 接收请求                │                      │
│     │                         │                      │
│     │────────────────────────▶│ ⑦ 返回响应           │
│     │ 发送响应                │                      │
│     │                         │                      │
│     │────────────────────────▶│ ⑧ 关闭连接           │
│     │ 关闭连接                │                      │
│                                                      │
└──────────────────────────────────────────────────────┘
```

---

## 第二部分：HTTP 协议详解

### 2.1 HTTP 是什么？

HTTP（超文本传输协议）是**文本型**的应用层协议：

```
请求和响应都是纯文本！

HTTP/1.1 200 OK\r\n
Content-Type: application/json\r\n
\r\n
{"status": "ok"}
```

### 2.2 HTTP 请求格式

```
GET /api/user HTTP/1.1\r\n        ← 请求行（方法 路径 协议）
Host: localhost:8080\r\n          ← 请求头（键: 值）
User-Agent: curl/7.68\r\n         ← 请求头
Accept: */*\r\n                   ← 请求头
\r\n                               ← 空行（表示头部结束）
                                 ← 请求体（GET 通常为空）
```

**常用 HTTP 方法：**
| 方法 | 用途 | 示例 |
|:---|:---|:---|
| GET | 获取资源 | GET /user |
| POST | 创建资源 | POST /user |
| PUT | 更新资源 | PUT /user/1 |
| DELETE | 删除资源 | DELETE /user/1 |

### 2.3 HTTP 响应格式

```
HTTP/1.1 200 OK\r\n              ← 状态行（协议 状态码 描述）
Content-Type: application/json\r\n ← 响应头
Content-Length: 27\r\n            ← 响应头
\r\n                              ← 空行
{"status": "ok", "id": 1}        ← 响应体
```

**常见状态码：**
| 状态码 | 含义 | 场景 |
|:---|:---|:---|
| 200 | OK | 请求成功 |
| 404 | Not Found | 资源不存在 |
| 500 | Internal Error | 服务器错误 |
| 400 | Bad Request | 请求格式错误 |

---

## 第三部分：Boost.Asio 基础

### 3.1 什么是 Boost.Asio？

Asio = **Asynchronous I/O**（异步输入输出）

它是一个跨平台的 C++ 网络库，提供：
- TCP/UDP 套接字
- 异步 I/O
- 定时器
- 串口通信

### 3.2 核心类

```cpp
#include <boost/asio.hpp>

// io_context: 应用程序和操作系统 I/O 服务的桥梁
boost::asio::io_context io;

// acceptor: 专门用于接受新连接的套接字
tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 8080));

// socket: 代表一个 TCP 连接
tcp::socket socket(io);
```

### 3.3 同步 vs 异步 I/O

**同步（阻塞）I/O：**
```cpp
// 程序停在这里，直到有客户端连接
acceptor.accept(socket);  // ← 阻塞！

// 程序停在这里，直到收到数据
socket.read_some(buffer); // ← 阻塞！
```

特点：
- 代码简单，顺序执行
- 同一时间只能处理一个连接
- 适合学习理解

**异步（非阻塞）I/O：**
```cpp
// 立即返回，不等待
acceptor.async_accept(socket, callback); // ← 不阻塞！

// 当连接到来时，callback 被调用
```

特点：
- 代码复杂，基于回调
- 可同时处理多个连接
- 适合生产环境

**本节使用同步 I/O**，便于理解核心概念。

---

## 第四部分：代码逐行解析

### 4.1 完整代码结构

```cpp
// 1. 包含头文件
#include <boost/asio.hpp>
#include <iostream>
#include <string>

// 2. 简化命名空间
using boost::asio::ip::tcp;

// 3. 构造 HTTP 响应的函数
std::string make_response(const std::string& body) { ... }

// 4. 主函数
int main() {
    // 4.1 创建 io_context
    // 4.2 创建 acceptor 并监听端口
    // 4.3 主循环：接受连接 → 读取请求 → 发送响应
    // 4.4 异常处理
}
```

### 4.2 make_response 函数详解

```cpp
std::string make_response(const std::string& body) {
    return "HTTP/1.1 200 OK\r\n"                    // 状态行
           "Content-Type: application/json\r\n"     // 内容类型
           "Content-Length: " +                     // 内容长度
           std::to_string(body.length()) + 
           "\r\n"                                   // 空行（头部结束）
           "\r\n" + body;                           // 响应体
}
```

**\r\n 是什么？**
- `\r` = 回车（Carriage Return）
- `\n` = 换行（Line Feed）
- HTTP 协议规定使用 `\r\n` 作为行结束符

### 4.3 main 函数详解

```cpp
int main() {
    try {
        // io_context 是 Asio 的核心
        boost::asio::io_context io;
        
        // acceptor 监听 8080 端口
        // tcp::v4() 表示 IPv4
        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 8080));
        
        std::cout << "Server listening on http://localhost:8080" << std::endl;
        
        // 无限循环，持续接受连接
        while (true) {
            // 创建 socket（连接还未建立）
            tcp::socket socket(io);
            
            // 阻塞等待客户端连接
            // 连接建立后，socket 代表这个连接
            acceptor.accept(socket);
            
            // 读取请求数据（简化版，只读 1024 字节）
            char buffer[1024] = {};
            size_t len = socket.read_some(boost::asio::buffer(buffer));
            
            // 构造响应
            std::string body = R"({"status": "ok"})";
            auto response = make_response(body);
            
            // 发送响应
            boost::asio::write(socket, boost::asio::buffer(response));
            
            // socket 析构，连接关闭
        }
        
    } catch (std::exception& e) {
        // 异常处理
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```

---

## 第五部分：动手实验

### 5.1 观察原始 HTTP 请求

修改代码，打印收到的请求：

```cpp
// 读取请求后添加
std::cout << "Raw request:\n" << std::string(buffer, len) << std::endl;
```

运行后用 curl 测试：
```bash
curl http://localhost:8080
```

你会看到类似：
```
Raw request:
GET / HTTP/1.1
Host: localhost:8080
User-Agent: curl/7.68.0
Accept: */*

```

### 5.2 返回不同的响应

修改 `body` 内容：

```cpp
std::string body = R"({
    "message": "Hello, World!",
    "timestamp": ")" + std::to_string(time(nullptr)) + R"("
})";
```

### 5.3 添加路由（简单版）

根据请求路径返回不同内容：

```cpp
// 解析请求路径
std::string request(buffer, len);
std::string body;

if (request.find("GET /hello") != std::string::npos) {
    body = R"({"message": "Hello!"})";
} else if (request.find("GET /time") != std::string::npos) {
    body = R"({"time": ")" + std::to_string(time(nullptr)) + "}";
} else {
    body = R"({"error": "Not found"})";
}
```

---

## 第六部分：常见问题

### Q1: 编译报错 `boost/asio.hpp: No such file`

**原因：** 没有安装 Boost 库

**解决：**
```bash
# Ubuntu/Debian
sudo apt-get install libboost-all-dev

# macOS
brew install boost
```

### Q2: 端口被占用

**错误：** `bind: Address already in use`

**解决：**
```bash
# 查看占用 8080 的进程
sudo lsof -i :8080

# 杀死进程
kill -9 <PID>

# 或更换端口
tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 8081));
```

### Q3: 为什么每次请求后连接都关闭？

**原因：** socket 在循环内定义，每次迭代结束时析构。

```cpp
while (true) {
    tcp::socket socket(io);  // 每次循环创建新 socket
    acceptor.accept(socket); // 接受连接
    // 处理请求...
} // socket 在这里析构，连接关闭！
```

**这是短连接模式**。Step 2 会学习 Keep-Alive 长连接。

### Q4: 为什么用 `while(true)`？

服务器需要持续运行，监听多个连接。

生产环境中，通常有**优雅退出**机制：
```cpp
std::atomic<bool> running{true};

// 捕获 Ctrl+C 信号
signal(SIGINT, []{ running = false; });

while (running) {  // 可以优雅退出
    // ...
}
```

---

## 下一步

→ **Step 1: 异步 I/O 与 CMake**

我们将学习：
- 异步编程模型
- 使用 CMake 构建项目
- Session 设计模式
