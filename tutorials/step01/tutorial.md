# Step 1: 异步 I/O 与 Session 管理 —— Agent 的并发感知

> 目标：实现异步非阻塞网络 I/O，让 Agent 能同时处理多个连接
> 
> 难度：⭐⭐ | 代码量：约 184 行 | 预计学习时间：2-3 小时

---

## 一、为什么需要异步 I/O？

### 1.1 Step 0 的瓶颈

Step 0 的 Echo 服务器使用**阻塞式 I/O**，存在严重的并发问题：

```
时间线：

T0:  Client A 连接 ──▶ accept() 返回，开始处理
T1:                    (处理中...)
T2:  Client B 连接 ──▶ 阻塞等待，无法响应
T3:                    (处理中...)
T4:  Client C 连接 ──▶ 阻塞等待，无法响应
T5:                    A 处理完成，响应发送
T6:                    B 的连接才被接受
```

**核心问题：**
- 处理一个客户端时，其他客户端必须等待
- 如果某个客户端网络慢，会拖慢所有人
- 无法支持实际生产环境的多用户场景

### 1.2 类比理解

想象一个餐厅的服务模式：

**阻塞式 I/O（Step 0）：**
```
服务员：您好，要点什么？
顾客A：我要一份牛排
服务员：好的（去厨房下单）
        ↓ 站在厨房等
        ↓ 15分钟后
服务员：（端菜回来）您的牛排
顾客A：吃完了，买单
服务员：好的（去收银台）
        ↓ 站在收银台等
        ↓ 5分钟后
顾客A：走了
服务员：终于可以服务顾客B了
```

**问题：** 服务员大部分时间在**等待**，而不是**工作**。

**异步 I/O 的目标：**
- 服务员下单后立即去服务其他顾客
- 厨房做好后通知服务员来端菜
- 一个服务员能同时服务多桌

---

## 二、异步编程核心概念

### 2.1 事件循环（Event Loop）

异步编程的核心是**事件循环**，它像一个调度中心：

```
┌─────────────────────────────────────────┐
│           Event Loop (io_context)       │
│                                         │
│   待处理事件队列：                        │
│   ┌─────────────────────────────────┐   │
│   │ 1. Socket A 可读（收到数据）      │   │
│   │ 2. Socket B 可写（可以发送）      │   │
│   │ 3. 定时器到期                     │   │
│   │ 4. Socket C 有新连接              │   │
│   └─────────────────────────────────┘   │
│                    │                    │
│                    ▼                    │
│   循环处理：取出事件 → 执行回调 → 回到等待 │
│                                         │
└─────────────────────────────────────────┘
```

**工作流程：**
1. 注册感兴趣的事件（如"当 socket 可读时通知我"）
2. 事件循环等待（不占用 CPU）
3. 事件发生，调用注册的回调函数
4. 回调执行完毕，继续等待下一个事件

### 2.2 回调机制

异步操作不会立即返回结果，而是通过**回调函数**在操作完成时通知：

```cpp
// 同步代码：立即得到结果
std::string data = socket.read();  // 阻塞等待
process(data);                     // 处理数据

// 异步代码：注册回调，稍后执行
socket.async_read(buffer, [](error_code ec, size_t bytes) {
    // 这个 lambda 在数据到达时被调用
    if (!ec) {
        process(buffer);
    }
});
// 立即返回，不等待！
```

**关键区别：**

| 特性 | 同步 | 异步 |
|:---|:---|:---|
| 执行顺序 | 线性，一行行执行 | 非线性，回调可能随时执行 |
| 等待方式 | 阻塞线程 | 注册回调，线程继续执行其他任务 |
| 代码复杂度 | 简单直观 | 相对复杂，需要理解回调时机 |
| 并发能力 | 一个线程一次一个 | 一个线程可同时处理多个 |

### 2.3 Boost.Asio 异步模型

Boost.Asio 使用 **Proactor 模式**：

```
应用层                      Asio 层                      OS 层
   │                          │                           │
   │ async_read(socket, cb)   │                           │
   │─────────────────────────▶│                           │
   │                          │ 告诉 OS：socket 可读时通知我 │
   │                          │──────────────────────────▶│
   │ 立即返回                  │                           │
   │                          │                           │
   │    （应用做其他事...）     │                           │
   │                          │                           │
   │                          │◀────────── 数据到达 ───────│
   │                          │                           │
   │                          │ 将回调加入事件队列          │
   │                          │                           │
   │ io_context.run()         │                           │
   │─────────────────────────▶│                           │
   │                          │ 取出回调并执行              │
   │                          │                           │
   │◀──────── 调用 cb(error, bytes) ──────────────────────│
   │                          │                           │
```

**核心组件：**

1. **`io_context`**：事件循环的核心，管理所有异步操作
2. **`async_*` 函数**：发起异步操作，立即返回
3. **回调函数**：操作完成时被调用，签名通常是 `(error_code, size_t)`
4. **`run()`**：启动事件循环，阻塞直到没有更多工作

---

## 三、Session 设计模式

### 3.1 为什么需要 Session？

Step 0 把所有逻辑写在 `main()` 里，随着功能增加会变得混乱。我们需要**面向对象的设计**：

```
Step 0 的混乱结构：                    Step 1 的清晰结构：

main() {                               class Session {
    while (true) {                         void do_read() {}
        accept(socket);                    void do_write() {}
        read(socket);                      void process() {}
        write(socket);                 };
        close(socket);
    }                                  class Server {
}                                        void accept() {}
                                     };
                                     
                                     main() {
                                         Server s(port);
                                         s.run();
                                     }
```

**Session 类的职责：**
- 管理一个客户端连接的生命周期
- 处理该连接的所有 I/O 操作
- 维护连接状态（读缓冲区、写缓冲区等）

### 3.2 Session 状态机

每个 Session 有自己的状态流转：

```
           新连接建立
               │
               ▼
    ┌──────────────────┐
    │     等待读取      │◀────────────────┐
    │  (do_read)       │                 │
    └────────┬─────────┘                 │
             │ 数据到达                   │
             ▼                           │
    ┌──────────────────┐                 │
    │     处理请求      │                 │
    │  (on_read)       │                 │
    └────────┬─────────┘                 │
             │                           │
             ▼                           │
    ┌──────────────────┐                 │
    │     等待写入      │                 │
    │  (do_write)      │                 │
    └────────┬─────────┘                 │
             │ 发送完成                   │
             ▼                           │
    ┌──────────────────┐                 │
    │     写完成回调    │─────────────────┘
    │  (on_write)      │  (保持连接，继续服务)
    └──────────────────┘
```

**关键点：** 处理完一个请求后，Session 不会销毁，而是继续等待下一个请求（HTTP Keep-Alive）。

---

## 四、核心代码实现

### 4.1 Session 类骨架

```cpp
#include <boost/asio.hpp>
#include <memory>
#include <iostream>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

// Session 继承 enable_shared_from_this
// 原因：异步回调需要保持 Session 存活，避免悬垂指针
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket) 
        : socket_(std::move(socket)) {}
    
    // 启动 Session，开始读取数据
    void start() {
        do_read();
    }

private:
    // 异步读取数据
    void do_read();
    
    // 读取完成回调
    void on_read(boost::system::error_code ec, std::size_t length);
    
    // 异步写入数据
    void do_write(std::size_t length);
    
    // 写入完成回调
    void on_write(boost::system::error_code ec, std::size_t length);

    tcp::socket socket_;
    char buffer_[1024];  // 读缓冲区
};
```

**关键技术点：**

1. **`enable_shared_from_this`**：
   - 异步回调可能在未来某个时刻执行
   - 如果 Session 对象已经被销毁，回调会访问非法内存
   - 使用 `shared_ptr` 管理生命周期，回调持有 `shared_ptr` 保证对象存活
   - `shared_from_this()` 在回调中创建新的 `shared_ptr` 引用

2. **`std::move(socket)`**：
   - Socket 是不可复制的（一个连接不能复制成两个）
   - 使用移动语义转移所有权

### 4.2 异步读取实现

```cpp
void Session::do_read() {
    // async_read_some：异步读取数据
    // 参数1：socket（数据从哪来）
    // 参数2：缓冲区（数据存到哪）
    // 参数3：回调函数（读完之后做什么）
    
    auto self = shared_from_this();  // 保持 Session 存活
    
    socket_.async_read_some(
        asio::buffer(buffer_),
        // 使用 bind 绑定 this 指针
        std::bind(
            &Session::on_read,
            self,  // shared_ptr 保证 Session 存活
            std::placeholders::_1,  // error_code
            std::placeholders::_2   // bytes_transferred
        )
    );
    
    // 函数立即返回，不等待数据！
}

void Session::on_read(boost::system::error_code ec, std::size_t length) {
    if (ec) {
        // 错误处理：客户端断开、网络错误等
        if (ec == asio::error::eof) {
            std::cout << "Client disconnected normally\n";
        } else {
            std::cerr << "Read error: " << ec.message() << "\n";
        }
        // Session 在这里自然销毁（shared_ptr 引用计数归零）
        return;
    }
    
    // 成功读取 length 字节数据
    std::cout << "Received " << length << " bytes\n";
    
    // 处理数据（这里简单 Echo）
    // ... 处理逻辑 ...
    
    // 发送响应
    do_write(length);
}
```

**关键理解：**

1. **`async_read_some` 立即返回**，不会阻塞等待数据
2. **回调在事件循环中执行**，可能在另一个线程（如果使用线程池）
3. **`shared_from_this()` 是关键**，它创建了一个 `shared_ptr`，回调持有了这个引用，保证即使外部释放，Session 也不会被销毁

### 4.3 异步写入实现

```cpp
void Session::do_write(std::size_t length) {
    auto self = shared_from_this();
    
    // async_write：保证把所有数据都发送完
    // 与 async_read_some 不同，async_write 会持续发送直到全部数据发出
    asio::async_write(
        socket_,
        asio::buffer(buffer_, length),  // 要发送的数据
        std::bind(
            &Session::on_write,
            self,
            std::placeholders::_1,
            std::placeholders::_2
        )
    );
}

void Session::on_write(boost::system::error_code ec, std::size_t length) {
    if (ec) {
        std::cerr << "Write error: " << ec.message() << "\n";
        return;
    }
    
    std::cout << "Sent " << length << " bytes\n";
    
    // 写完成后，继续等待读取下一个请求
    // 这是 HTTP Keep-Alive 的实现基础
    do_read();
}
```

### 4.4 Server 类实现

```cpp
class Server {
public:
    Server(asio::io_context& io, unsigned short port)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }

private:
    void do_accept() {
        // 创建一个 socket 准备接受新连接
        // 注意：socket 在堆上分配，因为 Session 需要长期持有
        auto socket = std::make_shared<tcp::socket>(acceptor_.get_executor().context());
        
        acceptor_.async_accept(
            *socket,
            [this, socket](boost::system::error_code ec) {
                if (!ec) {
                    std::cout << "New connection accepted\n";
                    
                    // 创建 Session 并启动
                    // make_shared 创建 shared_ptr，引用计数为 1
                    std::make_shared<Session>(std::move(*socket))->start();
                } else {
                    std::cerr << "Accept error: " << ec.message() << "\n";
                }
                
                // 继续接受下一个连接
                // 这是关键：服务器不会因为处理一个连接而停止接受新连接
                do_accept();
            }
        );
    }

    tcp::acceptor acceptor_;
};
```

**并发处理的关键：**

```
时间线：

T0:  async_accept 注册等待 ──▶ 立即返回
T1:  Client A 连接 ──▶ 回调执行，创建 Session A
T2:      └─▶ Session A::do_read 注册等待
T3:      └─▶ async_accept 再次注册等待（do_accept 递归调用）
T4:  Client B 连接 ──▶ 回调执行，创建 Session B
T5:      └─▶ Session B::do_read 注册等待
T6:      └─▶ async_accept 再次注册等待
T7:  Client A 数据到达 ──▶ Session A::on_read 执行
T8:  Client B 数据到达 ──▶ Session B::on_read 执行

结果：两个 Session 并发处理，互不阻塞！
```

---

## 五、CMake 构建系统

### 5.1 为什么需要 CMake？

Step 0 使用手动编译：
```bash
g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
```

**问题：**
- 依赖管理困难（Boost 库路径可能不同）
- 编译选项难以维护
- 不支持多文件项目
- 跨平台困难（Windows/Linux/macOS 命令不同）

### 5.2 CMake 基础

```cmake
cmake_minimum_required(VERSION 3.14)
project(AgentServer VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖库
find_package(Boost 1.70 REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)

# 添加可执行文件
add_executable(server 
    src/main.cpp
    src/server.cpp
    src/session.cpp
)

# 链接库
target_link_libraries(server 
    Boost::system
    Threads::Threads
)

# 包含目录
target_include_directories(server PRIVATE include)
```

### 5.3 构建流程

```bash
# 1. 创建构建目录（ out-of-source 构建）
mkdir build && cd build

# 2. 生成构建系统
cmake ..

# 3. 编译
make -j$(nproc)

# 4. 运行
./server
```

---

## 六、本章小结

**核心收获：**

1. **异步 I/O 原理**：理解事件循环、回调机制、Proactor 模式

2. **Session 设计模式**：
   - 每个连接对应一个 Session 对象
   - Session 管理自己的状态和生命周期
   - 使用 `shared_ptr` 和 `enable_shared_from_this` 保证异步安全

3. **并发处理能力**：
   - 单线程即可处理数千并发连接
   - 没有线程切换开销
   - I/O 等待期间 CPU 可以处理其他事件

4. **CMake 构建**：
   - 标准化的 C++ 项目构建方式
   - 支持依赖管理、跨平台构建

**关键代码模式：**

```
启动 Server ──▶ async_accept 等待连接
                    │
    连接到来 ─────▶ 创建 Session
                        │
                    Session::start()
                        │
                    do_read() ──▶ async_read_some
                                        │
                    数据到来 ───────────▶ on_read()
                                            │
                                        do_write() ──▶ async_write
                                                            │
                                        发送完成 ───────────▶ on_write()
                                                                    │
                                                                do_read()（循环）
```

---

## 七、引出的问题

### 7.1 协议解析问题

目前的 Session 只做了简单的 Echo：

```cpp
void on_read(...) {
    // 直接把收到的数据发回去
    do_write(length);
}
```

**问题：**
- 没有解析 HTTP 请求方法、路径、头部
- 无法根据 URL 路由到不同处理逻辑
- 不支持查询参数、POST 数据等

**这引出了下一章的核心问题：如何设计一个灵活的 HTTP 路由系统？**

### 7.2 回调地狱问题

随着功能增加，回调嵌套会越来越深：

```cpp
async_read(socket, buffer, [](ec, n) {
    async_parse_request(buffer, [](ec, req) {
        async_query_database(req, [](ec, data) {
            async_render_template(data, [](ec, html) {
                async_write(socket, html, [](ec, n) {
                    // 终于写完了！
                });
            });
        });
    });
});
```

**问题：** 代码可读性差，错误处理分散。

**未来解决方案：** C++20 coroutine（co_await），可以让异步代码看起来像同步代码。

---

**下一章预告（Step 2）：**

我们将构建 HTTP 协议解析器和路由系统：
- 完整解析 HTTP 请求（方法、路径、头部、正文）
- 设计 Router 类实现 URL 路由
- 支持不同的处理函数绑定到不同路径
- 为后续构建 REST API 打下基础

异步 I/O 解决了"同时服务多人"的问题，接下来要让 Agent 能"理解用户请求"。
