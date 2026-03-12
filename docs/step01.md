# Step 1: HTTP 路由系统

> 目标：添加 HTTP 路由和请求解析。
> 
> 代码量：约 100 行

## 新增功能

- HTTP 请求解析 (method, path, headers, body)
- 路由分发器 (不同路径不同处理)
- RESTful API 设计

## 核心概念

### HTTP 请求结构

```
GET /path HTTP/1.1\r\n
Host: localhost\r\n
Content-Type: application/json\r\n
Content-Length: 27\r\n
\r\n
{...body...}
```

### 路由系统

```cpp
class Router {
    std::map<std::string, Handler> routes_;
public:
    void add(const std::string& path, Handler handler);
    std::string handle(const HttpRequest& req);
};
```

## API 端点

| 方法 | 路径 | 功能 |
|------|------|------|
| GET | / | 服务状态 |
| GET | /health | 健康检查 |
| GET | /info | 请求信息 |
| POST | /chat | Echo 聊天 |

## 测试

```bash
# 服务状态
curl http://localhost:8080/

# 健康检查
curl http://localhost:8080/health

# Chat
curl -X POST http://localhost:8080/chat \
    -H "Content-Type: application/json" \
    -d '{"msg":"hello"}'

# 查看请求头信息
curl http://localhost:8080/info -H "X-Custom: value"
```

## 编译

```bash
g++ -std=c++17 main.cpp -o server \
    -lboost_system -lboost_thread -lpthread
```

## 下一步

→ [Step 2: 异步 I/O](step02.md)
