# Step 1: 异步 I/O 与 CMake 构建系统

> 目标：掌握异步编程模型，使用现代 CMake 构建项目
> 
> 难度：⭐⭐ (进阶)
> 
003e 代码量：约 120 行

## 本节收获

- 理解异步 I/O 的原理和优势
- 掌握智能指针和 lambda 表达式
- 学会使用 CMake 构建 C++ 项目
- 实现可扩展的并发服务器架构

---

## 第一部分：异步编程基础

### 1.1 为什么需要异步？

**同步 I/O 的问题：**

想象一家餐厅的服务员（服务器）：

```
同步模式（一个服务员）：

顾客A点餐 → 服务员等待厨房出餐 → 顾客A用餐 → 顾客A结账
    ↑                                              ↓
    └──────── 顾客B、C、D 都在排队等待 ─────────────┘

问题：服务员大部分时间都在"等待"，效率极低！
```

**异步 I/O 的解决方案：**

```
异步模式（一个服务员）：

顾客A点餐 ──→ 服务员记下订单 ──→ 去服务顾客B
    ↑                              ↓
    └── 厨房出餐 ──→ 服务员送餐给顾客A（回调）

优势：服务员从不等待，一直在工作，可同时服务多人
```

### 1.2 同步 vs 异步 对比表

| 特性 | 同步 I/O | 异步 I/O |
|:---|:---|:---|
| 代码难度 | 简单，顺序执行 | 复杂，基于回调 |
| 资源利用 | 阻塞等待，浪费 CPU | 非阻塞，高效利用 CPU |
| 并发能力 | 低（一个连接一个线程） | 高（一个线程多个连接） |
| 适用场景 | 简单任务、脚本 | 高并发服务器 |
| 调试难度 | 容易（堆栈清晰） | 困难（回调分散） |

### 1.3 异步 I/O 核心概念

**事件驱动模型：**

```
┌─────────────────────────────────────────────────────┐
│                  事件循环 (Event Loop)               │
├─────────────────────────────────────────────────────┤
│                                                     │
│  ① 注册事件监听器                                     │
│     socket.async_read(buffer, callback)             │
│              ↓                                      │
│  ② 立即返回，不阻塞                                  │
│              ↓                                      │
│  ③ 当数据到达时，操作系统通知                         │
│              ↓                                      │
│  ④ 事件循环调用 callback                             │
│              ↓                                      │
│  ⑤ 在 callback 中处理数据                            │
│                                                     │
└─────────────────────────────────────────────────────┘
```

**关键理解：**
- 异步操作**立即返回**，程序继续执行
- 真正的 I/O 由操作系统在后台完成
- I/O 完成后，**回调函数**被调用

---

## 第二部分：C++ 现代特性

### 2.1 智能指针 (Smart Pointers)

**为什么需要智能指针？**

```cpp
// 传统指针的问题：容易忘记释放，导致内存泄漏
void bad() {
    Session* session = new Session();  // 分配内存
    // ... 使用 session ...
    // 如果这里抛出异常或提前返回，内存泄漏！
    delete session;  // 可能执行不到
}
```

**智能指针自动管理内存：**

```cpp
#include <memory>

// unique_ptr: 独占所有权，不能复制
std::unique_ptr<Session> session = std::make_unique<Session>();
// 离开作用域时自动 delete

// shared_ptr: 共享所有权，引用计数
std::shared_ptr<Session> s1 = std::make_shared<Session>();
std::shared_ptr<Session> s2 = s1;  // 引用计数 +1
// 最后一个 shared_ptr 销毁时才 delete
```

### 2.2 enable_shared_from_this

**异步编程中的生命周期难题：**

```cpp
class Session {
    void do_read() {
        // 问题：lambda 捕获 this，但 Session 可能已被销毁！
        socket_.async_read(buffer_, [this](...) {
            // 悬垂指针！Segmentation fault!
        });
    }
};
```

**解决方案：**

```cpp
class Session : public std::enable_shared_from_this<Session> {
    void do_read() {
        // shared_from_this() 创建新的 shared_ptr，引用计数 +1
        auto self = shared_from_this();
        
        socket_.async_read(buffer_, [this, self](...) {
            // self 保持对象存活，直到回调执行完毕
        });
    }
};

// 创建时必须用 shared_ptr
auto session = std::make_shared<Session>(...);
session->start();
```

**生命周期图解：**

```
时间线 ─────────────────────────────────────────────▶

main()           async_read()        数据到达         回调完成
  │                   │                  │              │
  ▼                   ▼                  ▼              ▼
make_shared     创建 self           执行回调        self 销毁
  │              (ref=2)             (ref=1)        (ref=0)
  ▼                   │                  │              │
start()              │                  │              ▼
  │                  │                  │         Session 销毁
  ▼                  ▼                  ▼
ref=1 ────────── ref=2 ──────────▶ ref=1 ──────▶ ref=0
```

### 2.3 Lambda 表达式

Lambda 是 C++11 引入的**匿名函数**：

```cpp
// 基本语法：[捕获列表](参数列表) -> 返回类型 { 函数体 }

auto add = [](int a, int b) -> int {
    return a + b;
};

int result = add(1, 2);  // result = 3
```

**捕获列表：**

```cpp
int x = 10;

// [] - 不捕获任何外部变量
auto f1 = []() { return x; };  // 错误！x 未捕获

// [=] - 按值捕获所有外部变量
auto f2 = [=]() { return x; };  // OK，x 的拷贝

// [&] - 按引用捕获所有外部变量
auto f3 = [&]() { x = 20; };  // OK，修改原变量

// [this] - 捕获当前对象指针
auto f4 = [this]() { return member_; };  // OK

// [x, &y] - 混合捕获
auto f5 = [x, &y]() { y = x; };  // x 按值，y 按引用
```

**异步回调中的 Lambda：**

```cpp
void do_read() {
    auto self = shared_from_this();
    
    socket_.async_read(buffer_,
        // 这是一个 lambda，作为回调函数
        [this, self](boost::system::error_code ec, std::size_t len) {
            // this: 访问成员变量
            // self: 保持对象存活
            // ec: 错误码
            // len: 读取的字节数
            
            if (!ec) {
                process_data(len);
            }
        }
    );
}
```

---

## 第三部分：异步服务器架构

### 3.1 组件职责划分

```
┌─────────────────────────────────────────────────────────────┐
│                      异步服务器架构                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                 io_context (事件循环)                  │   │
│  │  • 管理所有异步操作                                     │   │
│  │  • 调度回调函数执行                                     │   │
│  │  • 线程安全                                             │   │
│  └──────────────────────────────────────────────────────┘   │
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
│                            │                                │
│                            ▼                                │
│                     ┌────────────┐                          │
│                     │   Router   │                          │
│                     │            │                          │
│                     │• URL 路由  │                          │
│                     │• 请求分发  │                          │
│                     └────────────┘                          │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 请求处理流程

```
① 新连接到达
        ↓
② Acceptor::async_accept 回调
        ↓
③ 创建 Session (shared_ptr 管理)
        ↓
④ Session::start() → do_read()
        ↓
⑤ async_read_some 注册读事件
        ↓
⑥ 数据到达，回调执行
        ↓
⑦ 解析请求 → Router 分发
        ↓
⑧ 生成响应 → do_write()
        ↓
⑨ async_write 发送数据
        ↓
⑩ 发送完成，Session 销毁或继续读取
```

### 3.3 多线程 io_context

**单线程 vs 多线程：**

```
单线程 io_context：

线程1: [回调A] → [回调B] → [回调C] → ...
       所有回调串行执行，无竞态条件

多线程 io_context：

线程1: [回调A] → [回调C] → ...
线程2: [回调B] → [回调D] → ...
线程3: [回调E] → ...
       回调并行执行，需要注意线程安全
```

**代码实现：**

```cpp
// 获取 CPU 核心数
unsigned thread_count = std::thread::hardware_concurrency();

// 创建线程池
std::vector<std::thread> threads;
for (unsigned i = 0; i < thread_count; ++i) {
    threads.emplace_back([&io]() {
        io.run();  // 每个线程都运行事件循环
    });
}

// 等待所有线程结束
for (auto& t : threads) {
    t.join();
}
```

**注意事项：**
- 多线程下，多个回调可能同时执行
- 共享数据需要加锁（或使用原子操作）
- Asio 的 `strand` 可以保证回调串行执行

---

## 第四部分：CMake 构建系统

### 4.1 为什么需要 CMake？

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

### 4.2 CMake 基础语法

```cmake
# CMakeLists.txt

# 最低版本要求
cmake_minimum_required(VERSION 3.14)

# 项目名称和语言
project(nuclaw_step01 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖库
find_package(Boost 1.70 REQUIRED COMPONENTS system thread)

# 添加可执行文件
add_executable(nuclaw_step01 main.cpp)

# 链接库
target_link_libraries(nuclaw_step01 
    Boost::system 
    Boost::thread
)
```

### 4.3 常用 CMake 命令

```bash
# 创建构建目录（重要！不要直接在源码目录构建）
mkdir build && cd build

# 生成构建文件
cmake ..

# 编译（使用所有 CPU 核心）
make -j$(nproc)

# 或使用 cmake 编译
cmake --build . --parallel

# Debug 模式
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release 模式（优化）
cmake -DCMAKE_BUILD_TYPE=Release ..
```

### 4.4 CMake 变量和选项

```cmake
# 设置变量
set(SOURCES main.cpp utils.cpp)

# 条件判断
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0)
else()
    add_compile_options(-O3)
endif()

# 添加 include 目录
target_include_directories(nuclaw_step01 PRIVATE include/)

# 添加编译选项
target_compile_options(nuclaw_step01 PRIVATE -Wall -Wextra)
```

---

## 第五部分：代码详解

### 5.1 请求解析

```cpp
HttpRequest parse_request(const std::string& raw) {
    HttpRequest req;
    std::istringstream stream(raw);
    std::string line;
    
    // 解析请求行：GET /path HTTP/1.1
    if (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        line_stream >> req.method >> req.path;
    }
    
    // 解析请求头
    while (std::getline(stream, line) && line != "\r") {
        auto pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            boost::trim(key);
            boost::trim(value);
            req.headers[key] = value;
        }
    }
    
    return req;
}
```

### 5.2 路由系统

```cpp
class Router {
public:
    // 定义处理函数类型
    using Handler = std::function<std::string(const HttpRequest&)>;
    
    // 注册路由
    void add(const std::string& path, Handler handler) {
        routes_[path] = handler;
    }
    
    // 处理请求
    std::string handle(const HttpRequest& req) {
        auto it = routes_.find(req.path);
        if (it != routes_.end()) {
            return it->second(req);  // 调用处理函数
        }
        return R"({"error":"not found"})";
    }
    
private:
    std::map<std::string, Handler> routes_;
};
```

### 5.3 Session 生命周期

```cpp
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, Router& router)
        : socket_(std::move(socket)), router_(router) {}
    
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
                    // 解析请求
                    auto req = parse_request(std::string(buffer_.data(), len));
                    // 路由处理
                    auto response = router_.handle(req);
                    // 发送响应
                    do_write(response);
                }
            }
        );
    }
    
    void do_write(const std::string& response) {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(response),
            [this, self](boost::system::error_code, std::size_t) {
                socket_.close();  // 短连接模式
            }
        );
    }
    
    tcp::socket socket_;
    Router& router_;
    std::array<char, 8192> buffer_;
};
```

---

## 第六部分：运行测试

### 6.1 编译运行

```bash
cd src/step01
mkdir build && cd build
cmake .. && make -j4
./nuclaw_step01
```

### 6.2 功能测试

```bash
# 根路由
curl http://localhost:8080/
# {"status":"ok","step":1,"features":"async+multithread"}

# 健康检查
curl http://localhost:8080/health
# {"health":"ok","threads":8}

# 404
curl http://localhost:8080/notexist
# {"error":"not found"}
```

### 6.3 并发测试

```bash
# 安装 Apache Bench
sudo apt-get install apache2-utils

# 10000 请求，100 并发
ab -n 10000 -c 100 http://localhost:8080/
```

**预期结果：**
- 吞吐量比 Step 0 高 10 倍以上
- 可以处理数千并发连接

---

## 常见问题

### Q: `shared_from_this()` 抛出 `bad_weak_ptr`

**原因：** 对象不是用 `shared_ptr` 创建的

**解决：**
```cpp
// 错误
Session session(socket, router);
session.start();

// 正确
auto session = std::make_shared<Session>(socket, router);
session->start();
```

### Q: CMake 找不到 Boost

**原因：** Boost 未安装或版本过低

**解决：**
```bash
# Ubuntu/Debian
sudo apt-get install libboost-all-dev

# 或指定 Boost 路径
cmake -DBOOST_ROOT=/opt/boost_1_83_0 ..
```

### Q: 回调函数没有被调用

**原因：** 忘记调用 `io.run()`

**解决：**
```cpp
asio::io_context io;
// ... 注册异步操作 ...
io.run();  // 启动事件循环！
```

### Q: 多线程下程序崩溃

**原因：** 共享数据没有线程保护

**解决：**
```cpp
// 使用 mutex 保护共享数据
std::mutex mutex_;
std::map<std::string, Session> sessions_;

void add_session(const std::string& id, Session s) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_[id] = s;
}
```

---

## 下一步

→ **Step 2: HTTP 长连接优化**

我们将学习：
- HTTP Keep-Alive 机制
- 连接复用与超时管理
- 性能优化技巧
