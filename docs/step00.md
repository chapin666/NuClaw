# Step 0: 最简单的 HTTP Echo 服务器

> 目标：创建一个最基本的 HTTP 服务器，能响应请求。
> 
> 代码量：约 50 行

## 本节收获

- 理解 Boost.Asio 的核心概念
- 掌握同步 TCP 服务器的编写
- 了解 HTTP 协议的最简实现

## 核心概念

### Boost.Asio 基础组件

```cpp
asio::io_context io;                    // 事件循环
tcp::acceptor acceptor(io, endpoint);   // 监听端口
tcp::socket socket(io);                 // 客户端连接
acceptor.accept(socket);                // 接受连接
```

### HTTP 协议简化

HTTP/1.1 最简单的响应格式：

```
HTTP/1.1 200 OK\r\n
Content-Type: application/json\r\n
Content-Length: 27\r\n
\r\n
{"status":"ok"}
```

## 完整代码

见 [src/step00/main.cpp](../src/step00/main.cpp)

## 逐行解析

### 1. 头文件

```cpp
#include <boost/asio.hpp>    // Asio 核心
#include <iostream>           // 标准输出
#include <string>             // 字符串处理
```

### 2. 创建响应

```cpp
std::string make_response(const std::string& body) {
    return "HTTP/1.1 200 OK\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: " + std::to_string(body.length()) + "\r\n"
           "\r\n" + body;
}
```

**关键点：**
- `\r\n` 是 HTTP 行结束符
- `Content-Length` 必须准确，否则客户端无法判断响应结束
- 空行 `\r\n` 分隔头部和 Body

### 3. 主函数

```cpp
int main() {
    try {
        boost::asio::io_context io;           // 创建 I/O 上下文
        
        // 创建 acceptor，监听 8080 端口
        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 8080));
        std::cout << "Server listening on http://localhost:8080" << std::endl;
        
        while (true) {
            tcp::socket socket(io);           // 创建 socket
            acceptor.accept(socket);           // 阻塞等待连接
            
            // 读取请求（简化：只读 1024 字节）
            char buffer[1024] = {};
            socket.read_some(boost::asio::buffer(buffer));
            
            // 构造响应
            std::string body = R"({"status":"ok","message":"Hello from NuClaw Step 0"})";
            auto response = make_response(body);
            
            // 发送响应
            boost::asio::write(socket, boost::asio::buffer(response));
        }
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
```

## 编译运行

### 安装依赖

**Ubuntu/Debian:**
```bash
sudo apt-get install libboost-all-dev
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

```bash
cd src/step00
g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
```

### 运行

```bash
./server
# 输出: Server listening on http://localhost:8080
```

### 测试

```bash
# 使用 curl
curl http://localhost:8080
# 输出: {"status":"ok","message":"Hello from NuClaw Step 0"}

# 使用浏览器访问 http://localhost:8080
```

## 设计决策分析

### 为什么用同步 I/O？

**优点：**
- 代码简单直观
- 易于理解和调试

**缺点：**
- 阻塞式，一次只能处理一个请求
- 无法利用多核

**适用场景：**
- 学习原型
- 低并发场景

### 为什么是短连接？

每次请求处理完就关闭连接，简单但效率低。

**下一步改进：**
- Step 2 实现异步 I/O 和长连接

## 常见问题

### Q: 编译报错 `boost/asio.hpp: No such file`

A: 安装 Boost 库：
```bash
sudo apt-get install libboost-all-dev  # Ubuntu
brew install boost                      # macOS
```

### Q: 运行后浏览器访问没响应

A: 检查：
1. 程序是否正常启动（看到监听消息）
2. 端口 8080 是否被占用：`lsof -i :8080`
3. 防火墙是否允许：`sudo ufw allow 8080`

### Q: 中文显示乱码

A: HTTP 响应需要指定编码：
```cpp
"Content-Type: application/json; charset=utf-8\r\n"
```

## 延伸思考

1. 如果同时有 100 个客户端请求，这个服务器会怎样？
2. 如何实现 HTTP 请求解析（获取 Path、Headers）？
3. 如何添加路由（不同 Path 返回不同内容）？

**这些问题将在 Step 1-2 中解答。**

## 下一步

→ [Step 1: HTTP 路由系统](step01.md)

---

**本节代码:** [src/step00/main.cpp](../src/step00/main.cpp)
