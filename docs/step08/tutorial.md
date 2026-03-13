# Step 8: Tools 系统（下）- 工具生态与安全沙箱

> 目标：实现实际可用的工具，包括 HTTP、文件操作和代码执行
> 
> 难度：⭐⭐⭐⭐ (较难)
> 
003e 代码量：约 640 行

## 本节收获

- 使用 Boost.Beast 实现 HTTP 客户端
- 设计带路径白名单的文件操作
- 理解安全沙箱的核心思想
- 实现代码执行的安全检查
- 掌握工具安全的最佳实践

---

## 第一部分：工具安全概述

### 1.1 Agent 工具的风险

Agent 调用工具 = 执行代码，存在安全风险：

| 工具类型 | 风险 | 攻击示例 |
|:---|:---|:---|
| **HTTP** | SSRF（服务器端请求伪造）| 访问内网服务、元数据接口 |
| **文件** | 路径遍历、信息泄露 | `../../../etc/passwd` |
| **代码** | 任意代码执行、资源耗尽 | `rm -rf /`、`while(1) fork()` |
| **数据库** | SQL 注入 | `' OR '1'='1` |

### 1.2 安全设计原则

```
┌─────────────────────────────────────────────────────────────┐
│                    工具安全金字塔                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Level 4: 沙箱执行                                           │
│           └── 容器隔离、资源限制                              │
│                                                             │
│  Level 3: 输入校验                                           │
│           └── 白名单、类型检查、范围限制                      │
│                                                             │
│  Level 2: 权限控制                                           │
│           └── 只读/读写权限、路径白名单                       │
│                                                             │
│  Level 1: 禁用危险操作                                       │
│           └── 关键字过滤、黑名单                             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**原则：多层防护，纵深防御**

---

## 第二部分：HTTP 工具安全

### 2.1 SSRF 攻击原理

```
攻击者 ──> Agent ──> HTTP Tool ──> http://localhost:8080/admin
                               ↑
                               └── 访问了内网服务！
```

**危害：**
- 访问内网管理接口
- 读取云服务商元数据（如 AWS EC2 metadata）
- 攻击内网其他服务

### 2.2 防护措施

```cpp
class HttpGetTool : public Tool {
    bool is_allowed_url(const string& url) {
        // 1. 只允许 HTTP（禁止 file://、ftp:// 等）
        if (url.find("http://") != 0 && 
            url.find("https://") != 0) {
            return false;
        }
        
        // 2. 禁止访问内网 IP
        if (url.find("localhost") != string::npos ||
            url.find("127.0.") != string::npos ||
            url.find("192.168.") != string::npos ||
            url.find("10.") != string::npos) {
            return false;
        }
        
        return true;
    }
};
```

### 2.3 HTTP 客户端实现

```cpp
class HttpGetTool : public Tool {
public:
    json::value execute(const json::object& args) override {
        string url = args.at("url").as_string();
        
        // 安全检查
        if (!is_allowed_url(url)) {
            return error("URL not in whitelist");
        }
        
        // 解析 URL
        auto [host, target] = parse_url(url);
        
        // 使用 Boost.Beast 发送 HTTP 请求
        asio::io_context ioc;
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);
        
        // 解析域名
        auto results = resolver.resolve(host, "80");
        stream.connect(results);
        
        // 构造 HTTP 请求
        http::request<http::string_body> req{http::verb::get, target, 11};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, "NuClaw-Agent/1.0");
        
        // 发送请求
        http::write(stream, req);
        
        // 读取响应
        beast::flat_buffer buffer;
        http::response<http::dynamic_body> res;
        http::read(stream, buffer, res);
        
        // 关闭连接
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
        
        // 返回结果
        json::object result;
        result["status"] = res.result_int();
        result["body"] = beast::buffers_to_string(res.body().data());
        return result;
    }
};
```

---

## 第三部分：文件操作安全

### 3.1 路径遍历攻击

```
正常请求：file_read("data/config.txt")

恶意请求：file_read("../../../etc/passwd")
              ↑
              └── 跳出允许目录，访问系统文件！
```

### 3.2 沙箱设计

```cpp
class Sandbox {
private:
    vector<fs::path> allowed_paths_;  // 允许访问的路径白名单
    
public:
    // 添加允许的路径
    void allow_path(const fs::path& path) {
        allowed_paths_.push_back(fs::canonical(path));
    }
    
    // 检查路径是否允许
    bool is_allowed(const fs::path& path) const {
        try {
            // 获取绝对路径
            fs::path canon = fs::canonical(path);
            
            // 检查是否在白名单内
            for (const auto& allowed : allowed_paths_) {
                // 检查 canon 是否以 allowed 开头（子目录）
                auto [it, _] = std::mismatch(
                    allowed.begin(), allowed.end(),
                    canon.begin(), canon.end()
                );
                if (it == allowed.end()) return true;
            }
        } catch (...) {
            return false;
        }
        return false;
    }
    
    // 路径安全基础检查
    static bool is_safe_path(const string& path) {
        // 禁止 .. 和绝对路径
        if (path.find("..") != string::npos) return false;
        if (!path.empty() && path[0] == '/') return false;
        return true;
    }
};
```

### 3.3 文件工具实现

```cpp
class FileTool : public Tool {
private:
    Sandbox sandbox_;
    
public:
    FileTool() {
        // 默认只允许访问 data/ 目录
        sandbox_.allow_path(fs::current_path() / "data");
    }
    
    json::value execute(const json::object& args) override {
        string operation = args.at("operation").as_string();
        
        if (operation == "read") return file_read(args);
        if (operation == "write") return file_write(args);
        if (operation == "list") return file_list(args);
        
        return error("Unknown operation: " + operation);
    }

private:
    json::value file_read(const json::object& args) {
        string path = args.at("path").as_string();
        
        // 多层安全检查
        if (!Sandbox::is_safe_path(path)) {
            return error("Path contains unsafe characters");
        }
        if (!sandbox_.is_allowed(path)) {
            return error("Access denied: outside allowed directory");
        }
        
        // 限制读取行数
        int limit = 100;
        if (args.contains("limit")) {
            limit = min(static_cast<int>(args.at("limit").as_int64()), 1000);
        }
        
        // 执行读取...
    }
};
```

---

## 第四部分：代码执行安全

### 4.1 代码执行的风险

Agent 执行用户代码是最危险的操作：

```python
# 用户提交的" innocent "代码
import os
os.system("rm -rf /")  # 删除整个文件系统！

# 或者
import socket, subprocess
# 建立反向 shell，黑客完全控制服务器
```

### 4.2 多层安全防护

```cpp
class CodeExecuteTool : public Tool {
public:
    json::value execute(const json::object& args) override {
        string code = args.at("code").as_string();
        string language = args.contains("language") 
            ? args.at("language").as_string() : "python";
        
        // 第 1 层：静态安全检查
        auto check = security_check(code, language);
        if (!check.first) {
            return error("Security check failed: " + check.second);
        }
        
        // 第 2 层：资源限制
        // timeout 5: 最多执行 5 秒
        // ulimit -v: 限制内存
        string cmd = "timeout 5 python3 -c '" + escape_shell(code) + "' 2>&1";
        
        // 第 3 层：执行并获取结果
        FILE* pipe = popen(cmd.c_str(), "r");
        // ... 读取输出
        
        int status = pclose(pipe);
        
        json::object result;
        result["success"] = (status == 0);
        result["output"] = output;
        result["exit_code"] = status;
        return result;
    }

private:
    // 静态安全检查
    pair<bool, string> security_check(const string& code, 
                                      const string& language) {
        // 禁止的危险关键字
        vector<string> blacklist = {
            "import os", "import sys", "__import__",
            "subprocess", "socket", "urllib",
            "open(", "file(", "exec(", "eval(",
            "rm -rf", "mkfs", "dd if"
        };
        
        for (const auto& bad : blacklist) {
            if (code.find(bad) != string::npos) {
                return {false, "Forbidden keyword: " + bad};
            }
        }
        
        // 限制代码长度（防止超大代码消耗资源）
        if (code.length() > 10000) {
            return {false, "Code too long (max 10000 chars)"};
        }
        
        return {true, ""};
    }
    
    // Shell 转义（防止命令注入）
    string escape_shell(const string& s) {
        string result;
        for (char c : s) {
            if (c == '\'') result += "'\"'\"'";
            else result += c;
        }
        return result;
    }
};
```

### 4.3 更安全的方案：容器沙箱

```bash
# 使用 Docker 容器隔离
docker run --rm \
    --network none \           # 禁止网络
    --read-only \              # 只读文件系统
    --memory 100m \            # 限制内存
    --cpus 0.5 \               # 限制 CPU
    --timeout 5 \              # 超时
    sandbox-image \
    python3 -c "user_code"
```

---

## 第五部分：工具生态系统

### 5.1 工具分类

| 类别 | 示例 | 安全级别 |
|:---|:---|:---|
| **只读工具** | 查询天气、搜索信息 | 低 |
| **文件工具** | 读/写文件 | 中 |
| **网络工具** | HTTP 请求 | 中 |
| **系统工具** | 执行命令 | 高 |
| **代码工具** | 执行代码 | 极高 |

### 5.2 权限分级

```cpp
enum class PermissionLevel {
    READ_ONLY,      // 只读工具（查询类）
    STANDARD,       // 标准工具（文件、HTTP）
    SENSITIVE,      // 敏感工具（支付、删除）
    ADMIN           // 管理工具（系统配置）
};

class ToolRegistry {
    bool can_execute(const string& user_role, 
                     const string& tool_name) {
        auto tool_level = get_tool_level(tool_name);
        auto user_level = get_user_level(user_role);
        return user_level >= tool_level;
    }
};
```

---

## 第六部分：完整运行测试

### 6.1 编译运行

```bash
cd src/step08
mkdir build && cd build
cmake .. && make
./nuclaw_step08
```

### 6.2 测试 HTTP 工具

```bash
wscat -c ws://localhost:8081

# 正常请求
> {"tool": "http_get", "args": {"url": "http://example.com"}}

# SSRF 攻击（应被拒绝）
> {"tool": "http_get", "args": {"url": "http://localhost:8080/admin"}}
< {"success": false, "error": "URL not in whitelist"}

# 内网 IP（应被拒绝）
> {"tool": "http_get", "args": {"url": "http://192.168.1.1"}}
< {"success": false, "error": "URL not in whitelist"}
```

### 6.3 测试文件沙箱

```bash
# 正常访问
> {"tool": "file", "args": {"operation": "read", "path": "data/test.txt"}}

# 路径遍历（应被拒绝）
> {"tool": "file", "args": {"operation": "read", "path": "../../../etc/passwd"}}
< {"success": false, "error": "Path contains unsafe characters"}
```

### 6.4 测试代码执行安全

```bash
# 正常代码
> {"tool": "code_execute", "args": {"code": "print(1+1)"}}
< {"success": true, "output": "2"}

# 危险代码（应被拒绝）
> {"tool": "code_execute", "args": {"code": "import os; os.system('rm -rf /')"}}
< {"success": false, "error": "Security check failed: Forbidden keyword: import os"}
```

---

## 第七部分：Tools 系统总结

### 7.1 演进历程

```
Step 6: 同步工具框架
   ├── Tool Schema 定义
   ├── 参数校验
   └── 注册表模式

Step 7: 异步执行
   ├── 回调机制
   ├── 并发控制
   └── 任务队列

Step 8: 工具生态
   ├── HTTP 客户端
   ├── 文件沙箱
   └── 代码执行安全
```

### 7.2 安全 checklist

- [ ] 参数类型校验
- [ ] 输入长度限制
- [ ] 关键字黑名单
- [ ] 路径白名单
- [ ] URL 白名单
- [ ] 资源限制（CPU、内存、时间）
- [ ] 权限分级
- [ ] 容器沙箱（生产环境）

---

## 下一步

→ **Step 9: LLM 集成**

连接真实的 LLM API（OpenAI、Claude）：
- HTTP 客户端调用 LLM API
- 解析 LLM 的工具调用请求
- 整合 Tool 系统到 Agent

实现真正的智能 Agent！
