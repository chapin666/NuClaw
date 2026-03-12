# Step 0 编译运行指南

## 快速开始

```bash
# 1. 确保安装了 Boost
sudo apt-get install libboost-all-dev

# 2. 编译
cd src/step00
g++ -std=c++17 main.cpp -o server -lboost_system -lpthread

# 3. 运行
./server

# 4. 测试（另一个终端）
curl http://localhost:8080
```

## 输出示例

```
========================================
  NuClaw Step 0 - HTTP Echo Server
========================================
Server listening on http://localhost:8080
Press Ctrl+C to stop

[+] New connection accepted
[+] Received 78 bytes
[+] Response sent
```

## 测试命令

```bash
# 基本测试
curl http://localhost:8080

# 带自定义 Header
curl -H "X-Custom: test" http://localhost:8080

# 查看 HTTP 响应头
curl -i http://localhost:8080
```
