# Step 2: HTTP 长连接优化 (Keep-Alive)

> 目标：实现 HTTP Keep-Alive，支持连接复用。
> 
> 代码量：约 180 行

## 本节收获

- HTTP Keep-Alive 机制
- 连接超时管理
- 请求解析优化

## 架构图

```
┌─────────────────────────────────────────────────────────────┐
│                    Step 2 Keep-Alive 架构                     │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   客户端                                                      │
│      │                                                      │
│      │ ① 第一次请求 (Connection: keep-alive)                 │
│      ▼                                                      │
│   ┌─────────────────┐                                       │
│   │   Session       │◄──────── 建立连接                      │
│   │   (保持连接)     │                                       │
│   └────────┬────────┘                                       │
│            │ ② 处理请求，发送响应                              │
│            │ ③ 不关闭连接！                                   │
│            │                                                │
│      ⑷ 第二次请求 ───────────► 复用同一连接                    │
│            │                                                │
│            │ ⑤ 处理请求，发送响应                              │
│            │                                                │
│   超时(30s) │                                                │
│   或        │                                                │
│   客户端关闭 ▼                                                │
│   ┌─────────────────┐                                        │
│   │   关闭连接       │                                        │
│   └─────────────────┘                                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## 核心改进

### 1. 检测 Keep-Alive 头部

```cpp
if (boost::iequals(key, "Connection") && 
    boost::iequals(value, "keep-alive")) {
    req.keep_alive = true;
}
```

### 2. 响应中添加 Connection 头部

```cpp
std::string conn_header = keep_alive 
    ? "Connection: keep-alive\r\n" 
    : "Connection: close\r\n";
```

### 3. 连接超时管理

```cpp
// 设置 30 秒超时
timer_.expires_after(30s);
timer_.async_wait([self](boost::system::error_code ec) {
    if (!ec) {
        self->socket_.close();  // 超时关闭
    }
});
```

## 编译运行

```bash
cd src/step02
mkdir build && cd build
cmake .. && make
./nuclaw_step02
```

## 测试 Keep-Alive

```bash
# 使用 HTTP/1.1 默认 Keep-Alive
curl -v http://localhost:8080/

# 查看响应头中的 Connection: keep-alive
```

## 下一步

→ **Step 3: Agent Loop 状态机**
