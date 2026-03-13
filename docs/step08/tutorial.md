# Step 8: Tools 系统（下）- 工具生态与安全沙箱

> 目标：实现实际可用的工具，包括 HTTP、文件操作和代码执行
> 
003e 代码量：约 640 行

## 本节收获

- 使用 Boost.Beast 实现 HTTP 客户端
- 实现带路径白名单的文件操作
- 理解安全沙箱设计
- 代码执行的安全检查

---

## 实际工具实现

### HTTP 工具

```cpp
class HttpGetTool : public Tool {
    json::value execute(const json::object& args) override {
        std::string url = args.at("url").as_string();
        
        // 安全检查
        if (!is_allowed_url(url)) {
            return error("URL not in whitelist");
        }
        
        // 使用 Boost.Beast 发送请求
        auto [host, target] = parse_url(url);
        http::request<http::string_body> req{http::verb::get, target, 11};
        // ... 发送和接收
    }
};
```

### 文件工具（带沙箱）

```cpp
class FileTool : public Tool {
    Sandbox sandbox_;
    
    json::value execute(const json::object& args) override {
        std::string path = args.at("path").as_string();
        
        // 安全检查
        if (!Sandbox::is_safe_path(path)) {
            return error("Unsafe path");
        }
        if (!sandbox_.is_allowed(path)) {
            return error("Access denied");
        }
        
        // 执行操作
    }
};
```

### 代码执行沙箱

```cpp
class CodeExecuteTool : public Tool {
    // 安全检查
    std::pair<bool, std::string> security_check(
        const std::string& code) {
        // 禁止危险关键字
        std::vector<std::string> blacklist = {
            "import os", "subprocess", "socket",
            "open(", "exec(", "eval("
        };
        
        for (const auto& bad : blacklist) {
            if (code.find(bad) != std::string::npos) {
                return {false, "Forbidden: " + bad};
            }
        }
        return {true, ""};
    }
    
    // 使用 timeout 限制执行时间
    std::string cmd = "timeout 5 python3 -c '...'";
};
```

---

## 安全考虑

| 工具 | 风险 | 防护措施 |
|:---|:---|:---|
| HTTP | SSRF（访问内网）| URL 白名单、禁止内网 IP |
| File | 路径遍历、信息泄露 | 路径检查、白名单目录 |
| Code | 代码注入、资源耗尽 | 关键字过滤、超时、资源限制 |

---

## 运行测试

```bash
cd src/step08
mkdir build && cd build
cmake .. && make
./nuclaw_step08

# 测试
wscat -c ws://localhost:8081

# 列出工具
> "list"

# HTTP 请求
> {\"tool\": \"http_get\", \"args\": {\"url\": \"http://example.com\"}}

# 文件操作
> {\"tool\": \"file\", \"args\": {\"operation\": \"list\", \"path\": \"data\"}}

# 代码执行（安全）
> {\"tool\": \"code_execute\", \"args\": {\"code\": \"print(1+1)\"}}
```

---

## 下一步

→ **Step 9: LLM 集成**

连接 OpenAI/Claude API，实现真正的智能 Agent。
