# Step 0: HTTP Echo 服务器

> 目标：创建一个最基本的 HTTP 服务器，能响应请求。
> 
> 代码量：约 50 行（不含注释）
> 
> 预计学习时间：30 分钟

---

## 本节收获

- 理解 Boost.Asio 的核心概念（io_context, acceptor, socket）
- 掌握同步 TCP 服务器的编写方法
- 了解 HTTP/1.1 协议的最简实现
- 学会使用 g++ 命令行编译 C++ 项目

---

## 核心概念

### Boost.Asio 三大核心组件

```cpp
asio::io_context io;                    // 事件循环：管理所有 I/O 操作
tcp::acceptor acceptor(io, endpoint);   // 监听器：绑定端口，接受连接
tcp::socket socket(io);                 // 套接字：代表一个 TCP 连接
acceptor.accept(socket);                // 阻塞等待：直到有客户端连接
```

### HTTP/1.1 响应格式

```
HTTP/1.1 200 OK\r\n                    ← 状态行（协议 状态码 状态文本）
Content-Type: application/json\r\n     ← 头部：内容类型
Content-Length: 27\r\n                 ← 头部：内容长度（字节数，必须准确）
\r\n                                   ← 空行：分隔头部和主体
{"status":"ok"}                      ← 主体：实际返回的数据
```

**关键细节：**
- `\r\n` 是 HTTP 规定的行结束符（CRLF）
- `Content-Length` 必须准确，否则客户端无法判断响应结束位置
- 空行 `\r\n` 是头部和主体的唯一分隔标志

---

## 完整代码

见 [src/step00/main.cpp](../src/step00/main.cpp)

---

## 逐行解析

### 1. 头文件

```cpp
#include <boost/asio.hpp>    // Asio 网络库核心
#include <iostream>           // 标准输入输出（std::cout, std::cerr）
#include <string>             // 字符串处理
```

### 2. HTTP 响应构造

```cpp
std::string make_response(const std::string& body) {
    return "HTTP/1.1 200 OK\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: " + std::to_string(body.length()) + "\r\n"
           "\r\n" + body;
}
```

**注意点：**
- `std::to_string()` 将整数转为字符串
- 字符串拼接使用 `+` 运算符

### 3. 主函数逻辑

```cpp
int main() {
    boost::asio::io_context io;                       // 创建 I/O 上下文
    tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 8080));  // 监听 8080 端口
    
    while (true) {
        tcp::socket socket(io);                       // 创建 socket
        acceptor.accept(socket);                       // 阻塞等待连接
        
        char buffer[1024] = {};                        // 读取缓冲区
        socket.read_some(boost::asio::buffer(buffer)); // 读取请求
        
        std::string response = make_response(body);    // 构造响应
        boost::asio::write(socket, boost::asio::buffer(response));  // 发送响应
    }  // socket 在这里析构，自动关闭连接
}
```

---

## 编译运行

### 安装 Boost

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

### 编译（g++ 命令行）

```bash
cd src/step00

# 编译
g++ -std=c++17 main.cpp -o server -lboost_system -lpthread

# 参数说明：
# -std=c++17    使用 C++17 标准
# -o server      输出可执行文件名为 server
# -lboost_system 链接 Boost.System 库
# -lpthread      链接 POSIX 线程库（Asio 需要）
```

### 运行

```bash
./server
```

**预期输出：**
```
========================================
  NuClaw Step 0 - HTTP Echo Server
========================================
Server listening on http://localhost:8080
Press Ctrl+C to stop

[+] New connection accepted
[+] Received 78 bytes
[+] Response sent
```

### 测试

**终端 1：保持服务器运行**

**终端 2：发送测试请求**

```bash
# 使用 curl
curl http://localhost:8080

# 预期输出：
# {
#     "status": "ok",
#     "step": 0,
#     "message": "Hello from NuClaw!",
#     "note": "This is the simplest HTTP server"
# }

# 或使用 wget
wget -qO- http://localhost:8080

# 或在浏览器中访问：http://localhost:8080
```

---

## 设计决策分析

### 为什么用同步 I/O？

| 优点 | 缺点 |
|:---|:---|
| 代码简单直观，易于理解 | 阻塞式，一次只能处理一个请求 |
| 无需考虑线程安全问题 | 无法利用多核 CPU |
| 适合学习原型 | 并发性能差 |

**适用场景：** 学习、原型验证、低并发服务

**下一步改进：** Step 1 引入 CMake，Step 2 实现异步 I/O

### 为什么是短连接？

每个 HTTP 请求处理完后立即关闭连接。

**优点：** 简单、资源释放及时  
**缺点：** 频繁创建/销毁连接，开销大

**下一步改进：** Step 2 实现 HTTP Keep-Alive 长连接

---

## 常见问题

### Q: 编译报错 `boost/asio.hpp: No such file`

**原因：** 系统未安装 Boost 库

**解决：**
```bash
# Ubuntu/Debian
sudo apt-get install libboost-all-dev

# macOS
brew install boost

# 验证安装
ls /usr/include/boost/asio.hpp  # 应该存在该文件
```

### Q: 运行后浏览器/ curl 没响应

**排查步骤：**

1. **检查服务器是否启动**
   ```bash
   ps aux | grep server
   # 应该看到 ./server 进程
   ```

2. **检查端口占用**
   ```bash
   lsof -i :8080
   # 或
   netstat -tlnp | grep 8080
   ```

3. **检查防火墙**
   ```bash
   sudo ufw allow 8080  # Ubuntu
   sudo firewall-cmd --add-port=8080/tcp  # CentOS
   ```

4. **检查日志输出**
   - 服务器是否正常打印 "Server listening..."
   - 请求时是否打印 "New connection accepted"

### Q: 中文显示乱码

**解决：** HTTP 响应添加 charset
```cpp
"Content-Type: application/json; charset=utf-8\r\n"
```

### Q: 如何停止服务器？

**方法：** 按 `Ctrl+C` 发送 SIGINT 信号

---

## 课后练习

### 练习 1：修改响应内容

修改代码，让服务器返回你的姓名和当前时间：
```json
{
    "name": "你的名字",
    "time": "2024-01-01 12:00:00"
}
```

**提示：** 使用 `<ctime>` 头文件获取当前时间

### 练习 2：统计访问次数

添加一个全局计数器，统计服务器接收了多少个请求：
```cpp
static int request_count = 0;  // 注意线程安全问题
```

### 练习 3：解析 HTTP 请求行

修改代码，解析客户端发送的 HTTP 请求的第一行：
```
GET /path HTTP/1.1
```

提取出 method（GET）、path（/path）、version（HTTP/1.1）

**提示：** 使用 `std::string::find()` 和 `substr()`

---

## 延伸思考

这些问题将在后续章节解答：

1. **如果同时有 100 个客户端请求，这个服务器会怎样？**
   - 答案：串行处理，第 100 个客户端需要等待前面 99 个处理完
   - 解决：Step 2 引入多线程和异步 I/O

2. **如何实现不同 URL 返回不同内容？**
   - 例如：`/hello` 返回问候，`/time` 返回时间
   - 解决：Step 1 引入 HTTP 路由系统

3. **如何保持连接不断开，支持多个请求？**
   - 解决：Step 2 实现 HTTP Keep-Alive

---

## 下一步

→ [Step 1: 异步 I/O 与 CMake 基础](step01.md)

在 Step 1 中，你将学习：
- 使用 CMake 管理项目构建
- 异步 I/O 的基本概念
- Session 设计模式

---

**本节代码:** [src/step00/main.cpp](../src/step00/main.cpp)  
**预计学习时间:** 30 分钟  
**代码量:** 约 50 行
