# Step 2: HTTP Keep-Alive 长连接优化

> 目标：实现 HTTP Keep-Alive，支持连接复用
> 
> 难度：⭐⭐ (进阶)
> 
> 代码量：约 180 行（较 Step 1 新增约 60 行）

---

## 问题引入

**Step 1 的问题：**

每个 HTTP 请求后都关闭连接，效率极低：

```
客户端              服务器
   │                  │
   ├──① 建立 TCP ────▶│  (三次握手 ~1-2ms)
   ├──② 发送请求 ────▶│
   │◀──③ 返回响应 ────┤
   ├──④ 关闭连接 ────▶│  (四次挥手 ~1-2ms)
   │                  │
   ├──① 建立 TCP ────▶│  (下一个请求，再次握手！)
   ...
```

**问题：** 100 个请求 = 100 次握手，浪费大量时间在建立/关闭连接上。

**本章目标：** 实现 Keep-Alive，让连接保持打开并复用。

---

## 解决方案

### HTTP Keep-Alive 协议

客户端发送 `Connection: keep-alive` 表示希望保持连接：

```http
GET /api/data HTTP/1.1
Connection: keep-alive        ← 请求保持连接
```

服务器响应同意保持：

```http
HTTP/1.1 200 OK
Connection: keep-alive        ← 同意保持
```

### Keep-Alive 的效果

```
客户端              服务器
   │                  │
   ├──① 建立 TCP ────▶│  (只建立一次)
   ├──② 请求 1 ──────▶│
   │◀──③ 响应 1 ──────┤
   ├──④ 请求 2 ──────▶│  (复用同一连接！)
   │◀──⑤ 响应 2 ──────┤
   ├──⑥ 请求 3 ──────▶│
   │◀──⑦ 响应 3 ──────┤
   ├──⑧ 超时关闭 ────▶│  (最后才关闭)
```

---

## 代码对比

### Step 1 的关键代码

```cpp
void do_write(std::size_t len) {
    auto self = shared_from_this();
    asio::async_write(socket_, asio::buffer(buffer_, len),
        [this, self](boost::system::error_code, std::size_t) {
            socket_.close();  // ← 写完立即关闭
        }
    );
}
```

### Step 2 的修改

**主要改动：**
1. 添加 HTTP 请求/响应解析
2. 检测 `Connection: keep-alive` 头部
3. 决定是否保持连接
4. 添加超时定时器

```cpp
// 新增：HTTP 请求结构
struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    bool keep_alive = false;
};

// 新增：解析 HTTP 请求
HttpRequest parse_request(const std::string& raw) {
    HttpRequest req;
    std::istringstream stream(raw);
    std::string line;
    
    // 解析请求行
    if (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        line_stream >> req.method >> req.path;
    }
    
    // 解析头部
    while (std::getline(stream, line) && line != "\r") {
        auto pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            // trim
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r") + 1);
            
            req.headers[key] = value;
            if (key == "Connection" && value == "keep-alive") {
                req.keep_alive = true;  // ← 标记保持连接
            }
        }
    }
    return req;
}

// 修改：Session 类
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket) 
        : socket_(std::move(socket)),
          timer_(socket_.get_executor()) {}  // ← 新增定时器

    void start() {
        do_read();
    }

private:
    void do_read() {
        auto self = shared_from_this();
        
        // 设置 30 秒超时
        timer_.expires_after(std::chrono::seconds(30));
        timer_.async_wait([self](boost::system::error_code ec) {
            if (!ec) {
                self->socket_.close();  // 超时关闭
            }
        });
        
        socket_.async_read_some(
            asio::buffer(buffer_),
            [this, self](boost::system::error_code ec, std::size_t len) {
                timer_.cancel();  // ← 有数据到达，取消超时
                if (!ec) {
                    auto req = parse_request(std::string(buffer_.data(), len));
                    do_response(req);
                }
            }
        );
    }

    void do_response(const HttpRequest& req) {
        // 构造响应
        std::string body = R"({"status":"ok","step":2})";
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        
        // 根据请求决定是否保持连接
        if (req.keep_alive) {
            response += "Connection: keep-alive\r\n";
        } else {
            response += "Connection: close\r\n";
        }
        response += "\r\n" + body;
        
        do_write(response, req.keep_alive);
    }

    void do_write(const std::string& response, bool keep_alive) {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(response),
            [this, self, keep_alive](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    if (keep_alive) {
                        do_read();  // ← 保持连接，继续读取下一个请求
                    }
                    // 否则让 Session 析构，自动关闭连接
                }
            }
        );
    }

    tcp::socket socket_;
    asio::steady_timer timer_;  // ← 新增超时定时器
    std::array<char, 1024> buffer_;
};
```

---

## 文件变更清单

| 文件 | 变更类型 | 说明 |
|:---|:---|:---|
| `main.cpp` | 修改 | 添加 HTTP 解析、Keep-Alive 逻辑、超时定时器 |

---

## 完整源码

```cpp
#include <boost/asio.hpp>
#include <iostream>
#include <sstream>
#include <memory>
#include <map>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    bool keep_alive = false;
};

HttpRequest parse_request(const std::string& raw) {
    HttpRequest req;
    std::istringstream stream(raw);
    std::string line;
    
    if (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        line_stream >> req.method >> req.path;
    }
    
    while (std::getline(stream, line) && line != "\r") {
        auto pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r") + 1);
            
            req.headers[key] = value;
            if (key == "Connection" && value == "keep-alive") {
                req.keep_alive = true;
            }
        }
    }
    return req;
}

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket) 
        : socket_(std::move(socket)),
          timer_(socket_.get_executor()) {}

    void start() {
        do_read();
    }

private:
    void do_read() {
        auto self = shared_from_this();
        
        timer_.expires_after(std::chrono::seconds(30));
        timer_.async_wait([self](boost::system::error_code ec) {
            if (!ec) {
                self->socket_.close();
            }
        });
        
        socket_.async_read_some(
            asio::buffer(buffer_),
            [this, self](boost::system::error_code ec, std::size_t len) {
                timer_.cancel();
                if (!ec) {
                    auto req = parse_request(std::string(buffer_.data(), len));
                    do_response(req);
                }
            }
        );
    }

    void do_response(const HttpRequest& req) {
        std::string body = R"({"status":"ok","step":2})";
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        
        if (req.keep_alive) {
            response += "Connection: keep-alive\r\n";
        } else {
            response += "Connection: close\r\n";
        }
        response += "\r\n" + body;
        
        do_write(response, req.keep_alive);
    }

    void do_write(const std::string& response, bool keep_alive) {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(response),
            [this, self, keep_alive](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    if (keep_alive) {
                        do_read();
                    }
                }
            }
        );
    }

    tcp::socket socket_;
    asio::steady_timer timer_;
    std::array<char, 1024> buffer_;
};

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
                do_accept();
            }
        );
    }

    tcp::acceptor acceptor_;
};

int main() {
    try {
        asio::io_context io;
        Server server(io, 8080);
        
        std::cout << "Step 2 Server (Keep-Alive) listening on port 8080...\n";
        io.run();
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
    return 0;
}
```

---

## 编译运行

```bash
cd src/step02
mkdir build && cd build
cmake .. && make -j4
./nuclaw_step02
```

测试连接复用：
```bash
# 使用 HTTP/1.1 默认 Keep-Aliven
curl -v http://localhost:8080/
# 注意响应头：Connection: keep-alive

# 使用 nc 发送多个请求
nc localhost 8080
GET / HTTP/1.1
Host: localhost
Connection: keep-alive

GET / HTTP/1.1
Host: localhost
Connection: close

# 你会看到两个响应，证明连接复用了！
```

---

## 本章总结

- ✅ 解决了 Step 1 的连接频繁建立/关闭问题
- ✅ 新增 HTTP 请求解析
- ✅ 添加 `Connection: keep-alive` 支持
- ✅ 实现连接超时管理（30秒）
- ✅ 代码从 120 行扩展到 180 行

---

## 课后思考

我们的服务器现在可以复用连接了，但返回的数据是固定的 JSON：

```json
{"status":"ok","step":2}
```

如果要返回复杂数据（比如用户信息包含姓名、年龄、地址），怎么办？

用字符串拼接？
```cpp
std::string response = "{\"name\":\"" + name + "\",\"age\":" + std::to_string(age) + "}";
// 容易出错！引号、转义、空值处理...
```

有没有标准的方式来序列化/反序列化结构化数据？

<details>
<summary>点击查看下一章 💡</summary>

**Step 3: JSON 序列化与结构化数据**

我们将学习：
- JSON 数据格式
- nlohmann/json 库的使用
- 结构化请求和响应

预期效果：用对象的方式处理数据，告别字符串拼接！

</details>
