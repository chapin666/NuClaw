# Step 1: 异步 I/O 与 CMake 构建系统

> 目标：掌握异步编程模型，使用现代 CMake 构建项目
> 
> 难度：⭐⭐ (进阶)
> 
> 代码量：约 120 行

---

## 问题引入

**从这一步开始，我们要构建一个 AI Agent 服务器。**

但传统的同步服务器有严重性能问题：

```
同步模式（一个线程一个连接）：

线程1: [处理连接A] ──等待I/O──▶ [继续处理A]
线程2: [处理连接B] ──等待I/O──▶ [继续处理B]
线程3: [处理连接C] ──等待I/O──▶ [继续处理C]

问题：1000个连接 = 1000个线程，大部分时间在等待！
```

**本章目标：** 用异步 I/O 实现单线程处理数千连接的高并发服务器。

---

## 核心概念

### 异步 I/O 的工作原理

```
异步模式（一个线程处理所有连接）：

事件循环: [注册A读事件] [注册B读事件] [注册C读事件]
              ↓
         等待任一事件（不阻塞）
              ↓
事件到达: [A有数据] ──▶ 回调处理A ──▶ 立即返回
              ↓
         继续等待其他事件
```

**关键优势：** 一个线程管理数千连接，CPU 不浪费在等待 I/O 上。

### 核心组件

| 组件 | 职责 |
|:---|:---|
| `io_context` | 事件循环，调度所有异步操作 |
| `acceptor` | 监听端口，接受新连接 |
| `socket` | 网络连接，读写数据 |
| `strand` | 保证回调串行执行（线程安全） |

---

## 代码实现

### 项目结构

```
src/step01/
├── CMakeLists.txt
└── main.cpp
```

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.14)
project(nuclaw_step01 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)

add_executable(nuclaw_step01 main.cpp)
target_link_libraries(nuclaw_step01 
    Boost::system
    Threads::Threads
)
```

### main.cpp

```cpp
#include <boost/asio.hpp>
#include <iostream>
#include <memory>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

// 会话类：处理单个连接
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket) : socket_(std::move(socket)) {}

    void start() {
        do_read();
    }

private:
    void do_read() {
        auto self = shared_from_this();
        socket_.async_read_some(
            asio::buffer(buffer_),
            [this, self](boost::system::error_code ec, std::size_t len) {
                if (!ec) {
                    do_write(len);
                }
            }
        );
    }

    void do_write(std::size_t len) {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(buffer_, len),
            [this, self](boost::system::error_code, std::size_t) {
                // 短连接：响应后立即关闭
                socket_.close();
            }
        );
    }

    tcp::socket socket_;
    std::array<char, 1024> buffer_;
};

// 服务器类：监听并接受连接
class Server {
public:
    Server(asio::io_context& io, unsigned short port)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket))->start();
                }
                do_accept();  // 继续接受下一个连接
            }
        );
    }

    tcp::acceptor acceptor_;
};

int main() {
    try {
        asio::io_context io;
        Server server(io, 8080);
        
        std::cout << "Server listening on port 8080...\n";
        io.run();  // 启动事件循环
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
    return 0;
}
```

---

## 编译运行

```bash
cd src/step01
mkdir build && cd build
cmake .. && make -j4
./nuclaw_step01
```

测试：
```bash
curl http://localhost:8080/
# 返回：你发送的请求内容（echo 服务器）
```

---

## 本章总结

- ✅ 使用 `io_context` 实现事件驱动
- ✅ 用 `shared_from_this` 管理异步对象生命周期
- ✅ 单线程处理并发连接
- ✅ 使用 CMake 管理构建

---

## 课后思考

我们的服务器目前每次请求后都关闭连接。想象一下：

- 加载一个网页需要 50 个资源文件
- 每个文件都要经历 TCP 三次握手
- 每次握手耗时 1-2ms

**仅建立连接就要浪费 50-100ms！**

能不能让连接保持打开，复用同一个 TCP 连接发送多个请求？

<details>
<summary>点击查看下一章 💡</summary>

**Step 2: HTTP Keep-Alive 长连接优化**

我们将学习：
- `Connection: keep-alive` 头部
- 定时器实现连接超时
- 请求循环处理

预期效果：连接复用后，50 个请求只需 1 次握手！

</details>
