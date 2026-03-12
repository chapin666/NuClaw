# Step 3: WebSocket + Agent Loop

> 目标：全双工通信和对话管理。
> 
> 代码量：约 200 行

## 新增功能

- WebSocket 协议支持
- Agent Loop 架构
- 会话状态管理

## WebSocket vs HTTP

| 特性 | HTTP | WebSocket |
|------|------|-----------|
| 连接 | 短连接 | 长连接 |
| 通信 | 请求-响应 | 全双工 |
| 开销 | 每次有头部 | 握手后无头部 |

## Agent Loop

```
User Input -> History -> LLM -> Response -> History
                ↑                           ↓
                └──────── Assistant ─────────┘
```

## 测试

```javascript
// 浏览器控制台
const ws = new WebSocket('ws://localhost:8081');
ws.onmessage = e => console.log(e.data);
ws.send('Hello!');
```

## 编译

```bash
g++ -std=c++17 main.cpp -o server \
    -lboost_system -lpthread
```

## 下一步

→ [Step 4: LLM 集成](step04.md)
