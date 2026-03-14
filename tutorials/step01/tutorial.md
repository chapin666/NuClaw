# Step 1: 异步 I/O 与 CMake 构建系统

> 目标：掌握异步编程模型，使用现代 CMake 构建项目
> 
> 难度：⭐⭐ | 代码量：约 120 行 | 预计学习时间：2-3 小时

---

## 一、问题引入：为什么需要异步 I/O？

### 1.1 同步服务器的困境

想象一下，你去一家餐厅吃饭，店里只有**一个服务员**：

```
同步服务模式：

时间轴 ───────────────────────────────────────────────────▶

顾客A: 点餐 ───────等待──────▶ 出餐 ───────等待──────▶ 上菜
                                      ↑
顾客B: ───────────────────────────────┘ (只能干等)
                                      ↑
顾客C: ───────────────────────────────┘ (也只能干等)
```

在这个场景中：
- 服务员（相当于 CPU）大部分时间都在**等待**
- 顾客（相当于客户端连接）排队干等
- 实际工作时间占比很低

**计算机中的类比：**
- 读取网络数据 ≈ 等待厨房出餐（耗时 1-100ms）
- CPU 计算 ≈ 服务员记录订单（耗时纳秒级）
- 网络延迟远大于 CPU 运算速度

### 1.2 传统解决方案的缺陷

**多线程方案：**
```
为每个连接创建一个线程：

线程1: 处理连接A ──等待I/O──▶ 继续处理A
线程2: 处理连接B ──等待I/O──▶ 继续处理B
线程3: 处理连接C ──等待I/O──▶ 继续处理C
...
线程1000: 处理连接Z ──等待I/O──▶ 继续处理Z
```

**问题：**
- 1000 个连接 = 1000 个线程
- 每个线程栈 8MB，1000 个线程 = 8GB 内存！
- 线程切换开销巨大（上下文切换需要保存/恢复寄存器状态）

### 1.3 异步 I/O 的优势

**核心思想：不要在等待 I/O 时阻塞线程**

```
异步服务模式：

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

---

## 二、核心概念详解

### 2.1 事件驱动架构

异步 I/O 的核心是**事件循环（Event Loop）**：

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

**核心组件：**

| 组件 | 类比 | 职责 |
|:---|:---|:---|
| `io_context` | 餐厅经理 | 管理所有事件，调度回调执行 |
| `acceptor` | 门童 | 监听端口，迎接新顾客（连接） |
| `socket` | 服务员 | 与顾客（客户端）直接交流 |
| `buffer` | 托盘 | 暂存数据 |
| `callback` | 任务清单 | I/O 完成后要执行的操作 |

### 2.2 Boost.Asio 核心类

**io_context** - 事件循环的核心：
```cpp
asio::io_context io;
// io.run() 启动事件循环，阻塞直到所有工作完成
```

**tcp::acceptor** - 监听器：
```cpp
tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 8080));
// 绑定到 0.0.0.0:8080，开始监听连接
```

**tcp::socket** - 连接套接字：
```cpp
tcp::socket socket(io);
// 用于与客户端通信
```

**异步操作函数：**
```cpp
// 异步接受连接
acceptor.async_accept(socket, callback);

// 异步读取数据
socket.async_read_some(buffer, callback);

// 异步写入数据
asio::async_write(socket, buffer, callback);
```

### 2.3 Lambda 回调与捕获

异步操作使用 Lambda 作为回调：

```cpp
socket.async_read_some(buffer,
    [this, self](boost::system::error_code ec, std::size_t len) {
        // this: 访问成员变量
        // self: shared_ptr，保证对象存活
        // ec: 错误码
        // len: 读取到的字节数
    }
);
```

**捕获列表说明：**
- `[this]` - 捕获当前对象指针，可以访问成员变量
- `[self]` - 捕获 shared_ptr，延长对象生命周期
- `[=]` - 值捕获所有变量
- `[&]` - 引用捕获所有变量

### 2.4 生命周期管理（难点）

**问题：异步回调可能在对象销毁后才执行！**

```cpp
// 危险代码示例
void handle_connection(tcp::socket socket) {
    Session session(std::move(socket));  // 栈上对象
    session.start();  // 启动异步读取
    // session 在这里销毁！
    // 但异步读取还在进行中...
    // 回调执行时，session 已经不存在了！
}  // 💥 崩溃！
```

**解决方案：shared_ptr + enable_shared_from_this**

```cpp
class Session : public std::enable_shared_from_this<Session> {
public:
    void start() {
        // shared_from_this() 创建新的 shared_ptr，引用计数 +1
        auto self = shared_from_this();
        
        socket_.async_read_some(buffer_,
            [this, self](error_code ec, size_t len) {
                // 即使原始 shared_ptr 被销毁，
                // 这个 lambda 还持有引用，对象不会销毁
            }
        );
    }
};

// 正确使用方式
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

## 三、CMake 构建系统详解

### 3.1 为什么需要 CMake？

**跨平台构建的挑战：**

不同平台使用不同的构建工具：
- Linux: Make (Makefile)
- Windows: Visual Studio (.sln, .vcxproj)
- macOS: Xcode (.xcodeproj)

**CMake 的解决方案：**

```
CMakeLists.txt ──▶ CMake ──▶ Makefile / .sln / Xcode
     ↑                ↓
  写一次          到处构建
```

你只需要写一份 `CMakeLists.txt`，CMake 会自动生成对应平台的构建文件。

### 3.2 CMake 基础语法

**最小 CMake 项目：**

```cmake
# 指定最低 CMake 版本
cmake_minimum_required(VERSION 3.14)

# 定义项目名称和使用的语言
project(nuclaw_step01 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加可执行文件
add_executable(nuclaw_step01 main.cpp)
```

**关键命令解释：**

| 命令 | 作用 |
|:---|:---|
| `cmake_minimum_required` | 指定最低 CMake 版本，确保功能兼容 |
| `project` | 定义项目名称、版本、描述、语言 |
| `set` | 设置变量值 |
| `CMAKE_CXX_STANDARD` | C++ 标准版本（11/14/17/20） |
| `CMAKE_CXX_STANDARD_REQUIRED` | 强制使用指定标准，不支持则报错 |
| `add_executable` | 定义可执行文件目标 |

### 3.3 查找和使用第三方库

**查找 Boost 库：**

```cmake
# 查找 Boost 库的 system 组件
find_package(Boost REQUIRED COMPONENTS system)

# 如果找不到，CMake 会报错并停止
```

**查找线程库：**

```cmake
find_package(Threads REQUIRED)
```

**链接库到目标：**

```cmake
target_link_libraries(nuclaw_step01
    Boost::system        # Boost.Asio 需要
    Threads::Threads     # 线程支持需要
)
```

**现代 CMake 方式（推荐）：**

```cmake
# 传统的旧方式（不推荐）
include_directories(/usr/include/boost)
link_libraries(boost_system)

# 现代方式（推荐）
target_link_libraries(target Boost::system)
```

现代 CMake 使用**目标（Target）**和**属性（Property）**，更清晰、更易于维护。

### 3.4 完整的 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.14)
project(nuclaw_step01 LANGUAGES CXX)

# C++ 标准设置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # 不使用编译器扩展

# 查找依赖
find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)

# 添加可执行文件
add_executable(nuclaw_step01 main.cpp)

# 链接库
target_link_libraries(nuclaw_step01
    Boost::system
    Threads::Threads
)

# 可选：设置编译选项
target_compile_options(nuclaw_step01 PRIVATE
    -Wall        # 启用所有警告
    -Wextra      # 启用额外警告
    -O2          # 优化级别
)
```

### 3.5 CMake 编译流程

**步骤 1：创建构建目录（重要！）**

```bash
# 不要直接在源码目录构建！
mkdir build
cd build
```

使用独立的构建目录的好处：
- 源码目录保持干净
- 可以同时维护多个构建配置（Debug/Release）
- 方便清理（直接删除 build 目录）

**步骤 2：生成构建文件**

```bash
cmake ..
```

`..` 指向源码目录（包含 CMakeLists.txt 的目录）。

CMake 会：
1. 读取 CMakeLists.txt
2. 检测编译器和系统
3. 查找依赖库
4. 生成 Makefile（或 Visual Studio 项目等）

**步骤 3：编译**

```bash
make -j$(nproc)
```

- `make` 执行编译
- `-j$(nproc)` 使用所有 CPU 核心并行编译（加速）

**完整命令：**

```bash
cd src/step01
mkdir build && cd build
cmake .. && make -j4
./nuclaw_step01
```

### 3.6 CMake 常用变量

| 变量 | 含义 |
|:---|:---|
| `CMAKE_BUILD_TYPE` | 构建类型：Debug/Release/RelWithDebInfo/MinSizeRel |
| `CMAKE_INSTALL_PREFIX` | 安装路径前缀 |
| `CMAKE_CXX_COMPILER` | C++ 编译器路径 |
| `CMAKE_SOURCE_DIR` | 源码目录 |
| `CMAKE_BINARY_DIR` | 构建目录 |

**指定构建类型：**

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
```

**指定安装路径：**

```bash
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
make install
```

---

## 四、代码结构详解

### 4.1 项目结构

```
src/step01/
├── CMakeLists.txt      # 构建配置
└── main.cpp            # 主程序（约 120 行）
```

### 4.2 Session 类详解

Session 类负责处理单个客户端连接：

```cpp
class Session : public std::enable_shared_from_this<Session> {
public:
    // 构造函数接收 socket，使用 move 语义转移所有权
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

    tcp::socket socket_;              // TCP 连接
    std::array<char, 1024> buffer_;   // 读取缓冲区
};
```

**关键点：**
- 继承 `enable_shared_from_this` 是为了在回调中安全地保持对象存活
- `async_read_some` 读取数据到 buffer
- `async_write` 写回数据
- 回调中使用 `self` 保证对象生命周期

### 4.3 Server 类详解

Server 类负责监听端口和接受连接：

```cpp
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
```

**关键点：**
- `acceptor_` 绑定到指定端口，开始监听
- `async_accept` 异步等待新连接
- 接受连接后创建 Session 处理
- 递归调用 `do_accept()` 循环接受更多连接

### 4.4 主函数详解

```cpp
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

**关键点：**
- `io_context` 是事件循环的核心
- `io.run()` 启动事件循环，程序在这里阻塞
- 使用 try-catch 捕获异常

---

## 五、执行流程详解

### 5.1 完整流程图

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
  │       │                                        │
  │       ▼                                        │
  │   [立即返回，等待连接] ◀───────────────────────┤
  │       │                                        │
  │       ▼                                        │
  │   新连接到达                                  │
  │       │                                        │
  │       ▼                                        │
  │   callback 执行                               │
  │       │                                        │
  │       ├──▶ make_shared<Session>()            │
  │       │          │                             │
  │       │          ▼                             │
  │       │      session->start()                  │
  │       │          │                             │
  │       │          ▼                             │
  │       │      do_read()                         │
  │       │          │                             │
  │       │          ├──▶ async_read_some()       │
  │       │          │       │                      │
  │       │          │       ▼                      │
  │       │          │   [立即返回]                │
  │       │          │       │                      │
  │       │          │       ▼                      │
  │       │          │   数据到达                  │
  │       │          │       │                      │
  │       │          │       ▼                      │
  │       │          └──▶ callback:                │
  │       │                   │                      │
  │       │                   └──▶ do_write()       │
  │       │                            │             │
  │       │                            └──▶ async_write()
  │       │                                     │    │
  │       │                                     ▼    │
  │       │                                 写入完成 │
  │       │                                     │    │
  │       │                                     ▼    │
  │       │                                 socket.close()
  │       │                                     │    │
  │       │                                     ▼    │
  │       │                                 Session 销毁
  │       │                                          │
  └──▶ do_accept() ──────────────────────────────────┘
```

### 5.2 关键点总结

| 阶段 | 说明 |
|:---|:---|
| 初始化 | 创建 io_context 和 Server |
| 监听 | acceptor 绑定端口，开始监听 |
| 接受 | async_accept 等待连接，不阻塞 |
| 读取 | async_read_some 等待数据，不阻塞 |
| 写入 | async_write 发送数据，不阻塞 |
| 关闭 | 写入完成后关闭连接，销毁 Session |
| 循环 | do_accept 递归调用，持续接受新连接 |

---

## 六、编译运行

### 6.1 编译步骤

```bash
cd src/step01
mkdir build && cd build
cmake .. && make -j4
```

### 6.2 运行服务器

```bash
./nuclaw_step01
# 输出：
# Server listening on port 8080...
# Press Ctrl+C to stop.
```

### 6.3 测试连接

```bash
# 在另一个终端
curl http://localhost:8080/
# 返回你发送的请求内容（echo 服务器）
```

### 6.4 性能测试

```bash
# 安装 Apache Bench
sudo apt-get install apache2-utils

# 10000 请求，100 并发
ab -n 10000 -c 100 http://localhost:8080/
```

---

## 七、常见问题

### Q1: `shared_from_this()` 抛出 `bad_weak_ptr`

**原因：** 对象不是用 `shared_ptr` 创建的

**错误代码：**
```cpp
Session session(socket);  // 栈上对象
session.start();  // 崩溃！
```

**正确代码：**
```cpp
auto session = std::make_shared<Session>(std::move(socket));
session->start();
```

### Q2: 回调没有被调用

**原因：** 忘记调用 `io.run()`

**解决：**
```cpp
io.run();  // 必须调用，否则事件循环不启动！
```

### Q3: 程序立即退出

**原因：** `io_context` 没有工作要做

**解决：** 确保在 `io.run()` 之前注册了异步操作

---

## 八、本章总结

**核心概念：**
- ✅ 异步 I/O：不等待，立即返回，回调处理结果
- ✅ `io_context`：事件循环，调度所有异步操作
- ✅ `shared_ptr` + `enable_shared_from_this`：管理异步对象生命周期
- ✅ CMake：跨平台构建系统

**代码结构：**
- ✅ Server：监听端口，接受连接
- ✅ Session：处理单个连接的生命周期

**构建系统：**
- ✅ CMake 基本语法
- ✅ 查找和使用第三方库
- ✅ 编译流程

---

## 九、课后思考

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
- `Connection: keep-alive` 协议详解
- 定时器实现连接超时管理
- 请求循环处理

预期效果：连接复用后，50 个请求只需 1 次握手！

</details>
