# Step 1: 异步 I/O 与 CMake 构建系统

> 目标：掌握异步编程模型，使用现代 CMake 构建项目
> > 难度：⭐⭐ (进阶)
> > 代码量：约 120 行
> > 预计学习时间：2-3 小时

---

## 问题引入

**我们要构建一个 AI Agent 服务器。**

但首先，让我们看看传统服务器的问题：

### 同步服务器的瓶颈

想象一家只有**一个服务员**的餐厅：

```
同步模式：

时间轴 ───────────────────────────────────────────────────▶

顾客A: 点餐 ──等待──▶ 出餐 ──等待──▶ 上菜 ──等待──▶ 结账
                         ↑
顾客B: ──────────────────┘ (排队等待)
                         ↑
顾客C: ──────────────────┘ (排队等待)
```

**问题：** 服务员（CPU）大部分时间都在**等待**（I/O操作），顾客（连接）排队干等。

在计算机中：
- 读取网络数据 ≈ 等待厨房出餐
- CPU 计算 ≈ 服务员工作
- 网络延迟 1-100ms，CPU 运算纳秒级

**同步服务器的资源利用率极低！**

### 传统解决方案的问题

**多线程方案：**
```
线程1: 处理连接A ──等待I/O──▶ 继续处理A
线程2: 处理连接B ──等待I/O──▶ 继续处理B
线程3: 处理连接C ──等待I/O──▶ 继续处理C
...
线程1000: 处理连接Z ──等待I/O──▶ 继续处理Z
```

**问题：**
- 1000个连接 = 1000个线程
- 每个线程栈 8MB，1000个线程 = 8GB内存！
- 线程切换开销大

**本章目标：** 使用异步 I/O，**一个线程处理数千连接**。

---

## 核心概念

### 什么是异步 I/O？

**异步的核心思想：** 不要在等待 I/O 时阻塞线程。

```
异步模式：

时间轴 ───────────────────────────────────────────────────▶

时刻1: 顾客A点餐 ──记录订单──▶ 立即服务顾客B
时刻2: 顾客B点餐 ──记录订单──▶ 立即服务顾客C
时刻3: 厨房出餐 ──▶ 回调：给A上菜 ──▶ 继续服务C
时刻4: 顾客C点餐完成 ──▶ 厨房出餐 ──▶ 回调：给B上菜
```

**关键优势：**
- 服务员（线程）从不等待，一直在工作
- 一个线程可以同时"服务"数千顾客
- 没有线程切换开销

### 事件驱动架构

```
┌─────────────────────────────────────────────────────────────┐
│                    事件驱动架构                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   ┌───────────────────────────────────────────────────┐    │
│   │              io_context (事件循环)                 │    │
│   │                                                   │    │
│   │   ① 注册事件监听器                                │    │
│   │      socket.async_read(buffer, callback)          │    │
│   │                                                   │    │
│   │   ② 立即返回，不阻塞 ◀── 关键点！                │    │
│   │                                                   │    │
│   │   ③ 操作系统在后台处理 I/O                        │    │
│   │                                                   │    │
│   │   ④ I/O 完成，事件到达                            │    │
│   │                                                   │    │
│   │   ⑤ 事件循环调用 callback                         │    │
│   │         ↓                                         │    │
│   │   ⑥ 在 callback 中处理数据                        │    │
│   │                                                   │    │
│   └───────────────────────────────────────────────────┘    │
│                            │                                │
│          ┌─────────────────┼─────────────────┐              │
│          │                 │                 │              │
│          ▼                 ▼                 ▼              │
│     ┌─────────┐      ┌──────────┐      ┌──────────┐        │
│     │Acceptor │      │ SessionA │      │ SessionB │        │
│     │         │      │          │      │          │        │
│     │• 监听   │      │• 读请求  │      │• 读请求  │        │
│     │• 接受   │───▶  │• 处理    │      │• 处理    │        │
│     │  连接   │      │• 写响应  │      │• 写响应  │        │
│     └─────────┘      └──────────┘      └──────────┘        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 核心组件详解

| 组件 | 类比 | 职责 |
|:---|:---|:---|
| `io_context` | 餐厅经理 | 管理所有事件，调度回调执行 |
| `acceptor` | 门童 | 监听端口，迎接新顾客（连接） |
| `socket` | 服务员 | 与顾客（客户端）直接交流 |
| `buffer` | 托盘 | 暂存数据 |
| `callback` | 任务清单 | I/O完成后要执行的操作 |

### 生命周期管理难题

**问题：** 异步回调可能在对象销毁后才执行！

```cpp
// 危险代码！
void handle_connection(tcp::socket socket) {
    Session session(std::move(socket));  // 栈上对象
    session.start();  // 启动异步读取
    // session 在这里销毁！
    // 但异步读取还在进行中...
    // 回调执行时，session 已经不存在了！
}  // 💥 崩溃！
```

**解决方案：`shared_ptr` + `enable_shared_from_this`**

```cpp
class Session : public std::enable_shared_from_this<Session> {
public:
    void start() {
        auto self = shared_from_this();  // 引用计数 +1
        socket_.async_read_some(buffer_,
            [this, self](error_code ec, size_t len) {  // self 保持对象存活
                // 即使原始 shared_ptr 被销毁，
                // 这个 lambda 还持有引用，对象不会销毁
            }
        );
    }
};

// 正确用法
auto session = std::make_shared<Session>(std::move(socket));
session->start();  // shared_ptr 在堆上，不会自动销毁
```

**生命周期图解：**

```
时间线 ────────────────────────────────────────────────────────▶

main()
  │
  ▼
make_shared<Session>()  
  │  ref_count = 1
  ▼
start()
  │  async_read_some
  │  lambda 捕获 shared_ptr
  ▼  ref_count = 2
return
  │  原始 shared_ptr 超出作用域
  │  ref_count = 1 (lambda 还持有)
  ▼
... 时间流逝 ...
  │
数据到达 ◀────────────────────────────────────────
  │                                              │
  ▼                                              │
callback 执行                                    │
  │  lambda 中的 self 仍然有效                   │
  │  可以安全访问 session 成员                   │
  ▼                                              │
lambda 销毁                                      │
  │  ref_count = 0                               │
  ▼                                              │
Session 销毁 ◀───────────────────────────────────┘
```

---

## CMake 构建系统

### 为什么需要 CMake？

**跨平台构建的挑战：**

| 平台 | 构建工具 | 项目文件 |
|:---|:---|:---|
| Linux | Make | Makefile |
| Windows | Visual Studio | .sln, .vcxproj |
| macOS | Xcode | .xcodeproj |

**CMake 的解决方案：**

```
CMakeLists.txt ──▶ CMake ──▶ Makefile / .sln / Xcode
     ↑                ↓
  写一次          到处构建
```

### CMake 基础

```cmake
# CMakeLists.txt

cmake_minimum_required(VERSION 3.14)  # 最低版本要求

project(nuclaw_step01 LANGUAGES CXX)  # 项目名称

set(CMAKE_CXX_STANDARD 17)            # C++ 标准
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system)  # 查找依赖
find_package(Threads REQUIRED)

add_executable(nuclaw_step01 main.cpp)  # 添加可执行文件

target_link_libraries(nuclaw_step01     # 链接库
    Boost::system 
    Threads::Threads
)
```

### 编译流程

```bash
# 1. 创建构建目录（不要直接在源码目录构建！）
mkdir build && cd build

# 2. 生成构建文件
cmake ..

# 3. 编译
make -j$(nproc)  # 使用所有 CPU 核心

# 4. 运行
./nuclaw_step01
```

---

## 代码实现

### 项目结构

```
src/step01/
├── CMakeLists.txt
└── main.cpp (120行)
```

### main.cpp

```cpp
#include <boost/asio.hpp>
#include <iostream>
#include <memory>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

// ============================================================
// Session 类：处理单个连接
// ============================================================
class Session : public std::enable_shared_from_this<Session> {
public:
    // 构造函数接收 socket（使用 move 语义转移所有权）
    explicit Session(tcp::socket socket) 
        : socket_(std::move(socket)) {}

    // 启动会话：开始异步读取
    void start() {
        do_read();
    }

private:
    // 异步读取数据
    void do_read() {
        // shared_from_this() 创建新的 shared_ptr，保证对象存活
        auto self = shared_from_this();
        
        socket_.async_read_some(
            asio::buffer(buffer_),  // 读取到 buffer_
            [this, self](boost::system::error_code ec, std::size_t len) {
                if (!ec) {
                    // 读取成功，写回相同数据（echo）
                    do_write(len);
                }
                // 如果出错，Session 对象会被销毁，连接自动关闭
            }
        );
        // 函数立即返回，不等待读取完成！
    }

    // 异步写入数据
    void do_write(std::size_t len) {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(buffer_, len),
            [this, self](boost::system::error_code, std::size_t) {
                // 写入完成，关闭连接（短连接模式）
                // Session 对象在这里销毁
            }
        );
    }

    tcp::socket socket_;           // TCP 连接
    std::array<char, 1024> buffer_;  // 读取缓冲区
};

// ============================================================
// Server 类：监听并接受连接
// ============================================================
class Server {
public:
    Server(asio::io_context& io, unsigned short port)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)) {
        do_accept();  // 开始接受连接
    }

private:
    // 异步接受新连接
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    // 创建 Session 并启动
                    std::make_shared<Session>(std::move(socket))->start();
                }
                // 继续接受下一个连接（循环）
                do_accept();
            }
        );
    }

    tcp::acceptor acceptor_;  // 监听器
};

// ============================================================
// 入口函数
// ============================================================
int main() {
    try {
        // io_context 是 Asio 的核心，管理所有 I/O 事件
        asio::io_context io;
        
        // 创建服务器，监听 8080 端口
        Server server(io, 8080);
        
        std::cout << "Server listening on port 8080...\n";
        std::cout << "Press Ctrl+C to stop.\n";
        
        // 启动事件循环
        // 这里会阻塞，直到所有 I/O 完成或 io.stop() 被调用
        io.run();
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
    return 0;
}
```

### 代码流程图

```
main()
  │
  ▼
io_context io ───────────────────────────────────┐
  │                                               │
  ▼                                               │
Server server(io, 8080)                           │
  │                                               │
  ├──▶ acceptor_.open()                          │
  ├──▶ acceptor_.bind(0.0.0.0:8080)              │
  ├──▶ acceptor_.listen()                        │
  │                                               │
  ▼                                               │
do_accept() ◀────────────────────────────────────┤
  │                                               │
  ├──▶ async_accept(socket, callback)            │
  │      │                                        │
  │      ▼                                        │
  │   [立即返回，等待连接] ◀───────────────────────┤
  │      │                                        │
  │      ▼                                        │
  │   新连接到达                                  │
  │      │                                        │
  │      ▼                                        │
  │   callback 执行                               │
  │      │                                        │
  │      ├──▶ make_shared<Session>()            │
  │      │          │                             │
  │      │          ▼                             │
  │      │      session->start()                  │
  │      │          │                             │
  │      │          ▼                             │
  │      │      do_read()                         │
  │      │          │                             │
  │      │          ├──▶ async_read_some()       │
  │      │          │      │                      │
  │      │          │      ▼                      │
  │      │          │   [立即返回，等待数据]       │
  │      │          │      │                      │
  │      │          │      ▼                      │
  │      │          │   数据到达                  │
  │      │          │      │                      │
  │      │          │      ▼                      │
  │      │          │   callback 执行             │
  │      │          │      │                      │
  │      │          │      ├──▶ do_write()       │
  │      │          │      │      │               │
  │      │          │      │      ▼               │
  │      │          │      │   async_write()     │
  │      │          │      │      │               │
  │      │          │      │      ▼               │
  │      │          │      │   [立即返回]         │
  │      │          │      │      │               │
  │      │          │      │      ▼               │
  │      │          │      │   写入完成          │
  │      │          │      │      │               │
  │      │          │      │      ▼               │
  │      │          │      └──▶ Session 销毁     │
  │      │          │                             │
  │      │          │                             │
  │      └──▶ do_accept() ────────────────────────┘
  │                   │
  └── (循环接受新连接) │
                      ▼
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
# 开两个终端

# 终端 1：启动服务器
./nuclaw_step01
# 输出：Server listening on port 8080...

# 终端 2：测试
curl http://localhost:8080/
# 返回你发送的请求内容（echo 服务器）
```

---

## 性能测试

```bash
# 安装 Apache Bench
sudo apt-get install apache2-utils

# 10000 请求，100 并发
ab -n 10000 -c 100 http://localhost:8080/
```

**预期结果：**
- 吞吐量：每秒处理数千请求
- 内存占用：远低于多线程方案

---

## 常见问题

### Q: `shared_from_this()` 抛出 `bad_weak_ptr`

**原因：** 对象不是用 `shared_ptr` 创建的

**解决：**
```cpp
// 错误
Session session(socket);  // 栈上对象
session.start();

// 正确
auto session = std::make_shared<Session>(std::move(socket));
session->start();
```

### Q: 回调没有被调用

**原因：** 忘记调用 `io.run()`

**解决：**
```cpp
io.run();  // 必须调用，否则事件循环不启动！
```

### Q: 程序立即退出

**原因：** `io_context` 没有工作要做

**解决：** 确保在 `io.run()` 之前注册了异步操作

---

## 本章总结

**核心概念：**
- ✅ 异步 I/O：不等待，立即返回，回调处理结果
- ✅ io_context：事件循环，调度所有异步操作
- ✅ shared_ptr：管理异步对象生命周期

**代码结构：**
- ✅ Server：监听端口，接受连接
- ✅ Session：处理单个连接的生命周期

**构建系统：**
- ✅ CMake 跨平台构建

---

## 课后思考

我们的服务器目前每次请求后都关闭连接。想象一下：

- 加载一个网页需要 50 个资源文件（HTML、CSS、JS、图片...）
- 每个文件都要经历 TCP 三次握手（~1-2ms）
- 每次请求后还要四次挥手关闭连接（~1-2ms）

**仅建立/关闭连接就要浪费 50-100ms！** 而实际数据传输可能只需要 0.1ms。

能不能让连接保持打开，复用同一个 TCP 连接发送多个请求？

<details>
<summary>点击查看下一章 💡</summary>

**Step 2: HTTP Keep-Alive 长连接优化**

我们将学习：
- `Connection: keep-alive` 协议
- 定时器实现连接超时管理
- 请求循环处理

预期效果：连接复用后，50 个请求只需 1 次握手！

</details>
