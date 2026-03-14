# Step 1: 异步 I/O 与 CMake 构建系统

> 目标：掌握异步编程模型，使用现代 CMake 构建项目
> 
> 难度：⭐⭐ (进阶) | 代码量：~120行 | 预计：2-3小时

---

## 问题引入

### 同步服务器的瓶颈

想象一家只有**一个服务员**的餐厅：

```
同步模式：
时间轴 ───────────────────────────────────────▶

顾客A: 点餐 ──等待──▶ 出餐 ──等待──▶ 上菜 ──等待──▶ 结账
                         ↑
顾客B: ──────────────────┘ (排队等待)
```

**问题：** 服务员（CPU）大部分时间都在**等待** I/O，顾客排队干等。

### 异步解决方案

```
异步模式：
时间轴 ───────────────────────────────────────▶

时刻1: A点餐 ──记录──▶ 立即服务B
时刻2: B点餐 ──记录──▶ 立即服务C  
时刻3: 厨房出餐 ──▶ 回调给A上菜 ──▶ 继续服务C
```

**一个线程处理数千连接！**

---

## 核心概念

### 事件驱动架构

```
┌─────────────────────────────────────────────┐
│              io_context (事件循环)           │
├─────────────────────────────────────────────┤
│  ① 注册事件监听器                             │
│     socket.async_read(buffer, callback)     │
│  ② 立即返回，不阻塞 ◀── 关键点！             │
│  ③ 操作系统后台处理 I/O                       │
│  ④ I/O完成，事件到达                          │
│  ⑤ 事件循环调用 callback                      │
│  ⑥ 在 callback 中处理数据                     │
└─────────────────────────────────────────────┘
```

| 组件 | 类比 | 职责 |
|:---|:---|:---|
| `io_context` | 餐厅经理 | 管理所有事件，调度回调 |
| `acceptor` | 门童 | 监听端口，迎接新连接 |
| `socket` | 服务员 | 与客户端直接交流 |
| `callback` | 任务清单 | I/O完成后执行的操作 |

### 生命周期管理

**问题：** 异步回调可能在对象销毁后才执行！

**解决方案：** `shared_ptr` + `enable_shared_from_this`

```cpp
class Session : public std::enable_shared_from_this<Session> {
public:
    void start() {
        auto self = shared_from_this();  // 引用计数+1
        socket_.async_read_some(buffer_,
            [this, self](error_code ec, size_t len) {
                // lambda 持有引用，对象不会销毁
            }
        );
    }
};

// 使用
auto session = std::make_shared<Session>(std::move(socket));
session->start();  // 堆上对象，不会自动销毁
```

**生命周期图解：**

```
make_shared() ──▶ ref_count=1
     │
     ▼
start() ──▶ async_read_some ──▶ lambda 捕获 self
     │                              │
     ▼                              ▼
return                        ref_count=2
     │                              │
     ▼                              ▼
original shared_ptr           数据到达
销毁, ref_count=1                 │
     │                              ▼
     ▼                        callback 执行
...等待...                    lambda 中的 self 有效
                                   │
                                   ▼
                             lambda 销毁, ref_count=0
                                   │
                                   ▼
                             Session 销毁
```

---

## 核心代码

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
find_package(Boost REQUIRED COMPONENTS system)

add_executable(nuclaw_step01 main.cpp)
target_link_libraries(nuclaw_step01 Boost::system)
```

### Session 类（核心）

```cpp
class Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(tcp::socket socket) 
        : socket_(std::move(socket)) {}

    void start() { do_read(); }

private:
    void do_read() {
        auto self = shared_from_this();
        
        socket_.async_read_some(
            asio::buffer(buffer_),
            [this, self](error_code ec, size_t len) {
                if (!ec) do_write(len);
            }
        );
    }

    void do_write(size_t len) {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(buffer_, len),
            [this, self](error_code, size_t) {
                socket_.close();  // 短连接：写完后关闭
            }
        );
    }

    tcp::socket socket_;
    array<char, 1024> buffer_;
};
```

### Server 类（核心）

```cpp
class Server {
public:
    Server(io_context& io, unsigned short port)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](error_code ec, tcp::socket socket) {
                if (!ec) {
                    // 创建 Session 并启动
                    make_shared<Session>(move(socket))->start();
                }
                do_accept();  // 循环接受新连接
            }
        );
    }

    tcp::acceptor acceptor_;
};
```

### 入口函数

```cpp
int main() {
    io_context io;
    Server server(io, 8080);
    
    cout << "Server listening on port 8080...\n";
    io.run();  // 启动事件循环
    return 0;
}
```

---

## 编译运行

```bash
mkdir build && cd build
cmake .. && make -j4
./nuclaw_step01
```

测试：
```bash
curl http://localhost:8080/
```

---

## 流程图

```
main()
  │
  ▼
io_context io
  │
  ▼
Server server(io, 8080)
  │
  ├──▶ acceptor_.bind(0.0.0.0:8080)
  ├──▶ acceptor_.listen()
  │
  ▼
do_accept()
  │
  ├──▶ async_accept(socket, callback)
  │       │
  │       ▼
  │   [立即返回，等待连接]
  │       │
  │       ▼
  │   新连接到达
  │       │
  │       ▼
  │   callback:
  │       ├──▶ make_shared<Session>()
  │       │          │
  │       │          ▼
  │       │      session->start()
  │       │          │
  │       │          ▼
  │       │      do_read()
  │       │          │
  │       │          ├──▶ async_read_some()
  │       │          │       │
  │       │          │       ▼
  │       │          │   [立即返回]
  │       │          │       │
  │       │          │       ▼
  │       │          │   数据到达
  │       │          │       │
  │       │          │       ▼
  │       │          └──▶ callback:
  │       │                   │
  │       │                   └──▶ do_write()
  │       │                            │
  │       │                            └──▶ async_write()
  │       │                                     │
  │       │                                     ▼
  │       │                                 写入完成
  │       │                                     │
  │       │                                     ▼
  │       │                                 socket.close()
  │       │                                     │
  │       │                                     ▼
  │       │                                 Session 销毁
  │       │
  └──▶ do_accept() ──▶ (循环接受新连接)
```

---

## 常见问题

| 问题 | 原因 | 解决 |
|:---|:---|:---|
| `shared_from_this()` 抛异常 | 对象不是用 `shared_ptr` 创建 | 使用 `make_shared` |
| 回调没有被调用 | 忘记调用 `io.run()` | 确保调用 `io.run()` |
| 程序立即退出 | `io_context` 没有工作 | 在 `run()` 前注册异步操作 |

---

## 本章总结

- ✅ 异步 I/O：不等待，立即返回，回调处理结果
- ✅ `io_context`：事件循环，调度异步操作
- ✅ `shared_ptr`：管理异步对象生命周期

---

## 课后思考

我们的服务器每次请求后都关闭连接。想象一下：
- 加载一个网页需要 50 个资源文件
- 每个文件都要经历 TCP 三次握手（~1-2ms）
- 每次请求后还要四次挥手关闭连接（~1-2ms）

**仅建立/关闭连接就要浪费 50-100ms！** 能不能让连接保持打开，复用同一个 TCP 连接？

<details>
<summary>点击查看下一章 💡</summary>

**Step 2: HTTP Keep-Alive 长连接优化**

我们将学习：
- `Connection: keep-alive` 协议
- 定时器实现连接超时管理
- 请求循环处理

预期效果：连接复用后，50 个请求只需 1 次握手！

</details>
