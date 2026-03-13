# Step 8: 工具安全与沙箱 - 基于 Step 7 演进

> 目标：为工具添加安全保护，防止恶意操作
> 
> 难度：⭐⭐⭐⭐ (较难)
> 
> 代码量：约 700 行（分散在 14 个文件中）

## 📁 代码结构说明（演进式）

Step 8 基于 Step 7 结构，**新增安全相关文件**：

```
src/step08/                    src/step07/
├── main.cpp              (修改)     main.cpp
├── tool.hpp              (相同)     tool.hpp
├── weather_tool.hpp      (相同)     weather_tool.hpp
├── time_tool.hpp         (相同)     time_tool.hpp
├── calc_tool.hpp         (相同)     calc_tool.hpp
├── tool_executor.hpp     (演进)     tool_executor.hpp
├── llm_client.hpp        (相同)     llm_client.hpp
├── chat_engine.hpp       (相同)     chat_engine.hpp
├── server.hpp            (相同)     server.hpp
├── sandbox.hpp           (新增)     [无]
├── http_tool.hpp         (新增)     [无]
├── file_tool.hpp         (新增)     [无]
└── code_tool.hpp         (新增)     [无]
```

### 与 Step 7 的关系

**演进统计：**
- 10 个文件**相同**（从 Step 7 复制）
- 1 个文件**演进**（`tool_executor.hpp` 添加安全检查）
- 4 个文件**新增**（安全相关）
- 1 个文件**重写**（`main.cpp` 演示代码）

**演进对比：**

```cpp
// Step 7 tool_executor.hpp - 无安全检查
class ToolExecutor {
    static ToolResult execute_tool(const ToolCall& call) {
        if (call.name == "get_weather") 
            return WeatherTool::execute(call.args);
        // ... 其他工具
    }
};

// Step 8 tool_executor.hpp - 带安全检查
class ToolExecutor {
    static ToolResult execute_tool_with_safety(const ToolCall& call) {
        // Step 6 原有工具
        if (call.name == "get_weather") 
            return WeatherTool::execute(call.args);
        
        // Step 8 新增工具（带安全检查）
        if (call.name == "http_get") {
            // 自动调用 HttpTool::execute，内部有 SSRF 检查
            return HttpTool::execute(call.args);  
        }
        if (call.name == "file") {
            // 自动调用 FileTool::execute，内部有路径检查
            return FileTool::execute("read", call.args);
        }
        if (call.name == "code_execute") {
            // 自动调用 CodeTool::execute，内部有黑名单检查
            return CodeTool::execute(call.args);
        }
    }
};
```

### 新增文件说明

```cpp
// sandbox.hpp - 安全沙箱（核心新增）
class Sandbox {
    static bool is_safe_url(const std::string& url);      // SSRF 防护
    static bool is_safe_path(const std::string& path);    // 路径遍历防护
    static bool is_safe_code(const std::string& code);    // 代码注入防护
};

// http_tool.hpp - HTTP 工具（使用沙箱）
class HttpTool {
    static ToolResult execute(const std::string& url) {
        if (!Sandbox::is_safe_url(url)) {  // ← 安全检查
            return ToolResult::fail("SSRF: 禁止访问内网地址");
        }
        // ... 执行 HTTP 请求
    }
};

// file_tool.hpp - 文件工具（使用沙箱）
class FileTool {
    static ToolResult execute(const std::string& op, const std::string& path) {
        if (!Sandbox::is_safe_path(path)) {  // ← 安全检查
            return ToolResult::fail("Path Traversal: 禁止 .. 或绝对路径");
        }
        // ... 执行文件操作
    }
};

// code_tool.hpp - 代码执行（使用沙箱）
class CodeTool {
    static ToolResult execute(const std::string& code) {
        if (!Sandbox::is_safe_code(code)) {  // ← 安全检查
            return ToolResult::fail("Dangerous Code: 包含黑名单关键词");
        }
        // ... 执行代码
    }
};
```

### 为什么新增这些文件？

**安全是工具系统的必备要素：**

| 工具类型 | 风险 | 防护措施 |
|:---|:---|:---|
| HTTP 请求 | SSRF（访问内网） | URL 白名单 |
| 文件操作 | 路径遍历（读取敏感文件） | 路径规范化 |
| 代码执行 | 代码注入（执行危险命令） | 黑名单过滤 |

**文件分离的好处：**
1. **职责清晰**：每个文件只负责一种安全/工具
2. **可测试**：单独测试安全函数
3. **可复用**：Step 9+ 继续使用这些安全机制

---

## 本节收获

- 理解工具安全的重要性
- 掌握 SSRF、路径遍历、代码注入防护
- 实现安全沙箱机制
- 学会安全与功能的平衡

---

## 第一步：回顾 Step 7 的安全隐患

Step 7 实现了异步工具执行，但没有安全控制：

```cpp
// 危险的工具调用场景：

// 1. SSRF 攻击
用户：访问 http://localhost/admin
AI：调用 http_get("http://localhost/admin")
→ 可能访问到内部管理接口！

// 2. 路径遍历攻击  
用户：读取 ../../../etc/passwd
AI：调用 file("read", "../../../etc/passwd")
→ 可能读取到系统敏感文件！

// 3. 代码注入攻击
用户：执行 "import os; os.system('rm -rf /')"
AI：调用 code_execute("import os...")
→ 可能执行危险命令！
```

### 安全风险总结

| 攻击类型 | 危害 | 示例 |
|:---|:---|:---|
| SSRF | 访问内网服务 | `http://localhost:3306` 访问数据库 |
| 路径遍历 | 读取任意文件 | `../../../etc/shadow` 读取密码 |
| 代码注入 | 执行任意代码 | `os.system('rm -rf /')` 删除文件 |

---

## 第二步：安全沙箱设计

### 核心思想

**最小权限原则**：工具只能做它应该做的事，不能越界。

```cpp
// sandbox.hpp - 统一的安全检查入口
class Sandbox {
public:
    // URL 安全检查（防止 SSRF）
    static bool is_safe_url(const std::string& url) {
        // 禁止内网地址
        if (url.find("localhost") != npos) return false;
        if (url.find("127.0.") != npos) return false;
        if (url.find("192.168.") != npos) return false;
        if (url.find("10.") != npos) return false;
        
        // 只允许 http/https
        if (!starts_with(url, "http://") && !starts_with(url, "https://")) {
            return false;
        }
        
        return true;
    }
    
    // 路径安全检查（防止路径遍历）
    static bool is_safe_path(const std::string& path) {
        // 禁止 .. 遍历
        if (path.find("..") != npos) return false;
        
        // 禁止绝对路径（简化）
        if (!path.empty() && path[0] == '/') return false;
        
        return true;
    }
    
    // 代码安全检查（防止代码注入）
    static bool is_safe_code(const std::string& code) {
        // 黑名单检查
        vector<string> blacklist = {
            "import os", "import sys", "__import__",
            "open(", "file(", "exec(", "eval(",
            "subprocess", "socket", "urllib",
            "rm -rf", "mkfs", "dd if"
        };
        
        for (const auto& bad : blacklist) {
            if (code.find(bad) != npos) return false;
        }
        
        // 限制代码长度
        if (code.length() > 10000) return false;
        
        return true;
    }
};
```

### 安全金字塔

```
          ┌─────────────┐
          │   应用层    │  Agent 逻辑
          │  (Agent)    │
          ├─────────────┤
          │   工具层    │  Tool 实现
          │   (Tool)    │  调用 Sandbox 检查
          ├─────────────┤
          │   沙箱层    │  Sandbox 检查
          │ (Sandbox)   │  URL/路径/代码检查
          ├─────────────┤
          │   系统层    │  操作系统限制
          │   (System)  │  容器、权限、资源限制
          └─────────────┘
```

---

## 第三步：安全工具实现

### HTTP 工具（SSRF 防护）

```cpp
// http_tool.hpp
#include "sandbox.hpp"

class HttpTool {
public:
    static ToolResult execute(const std::string& url) {
        // 第一层：沙箱检查
        if (!Sandbox::is_safe_url(url)) {
            return ToolResult::fail(
                "🛡️  SSRF 防护：禁止访问内网或非 HTTP 协议\n"
                "违规 URL: " + url
            );
        }
        
        // 第二层：执行请求
        try {
            // ... 发送 HTTP 请求
            return ToolResult::ok(response_json);
        } catch (...) {
            return ToolResult::fail("HTTP 请求失败");
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
    static ToolResult execute(const std::string& operation,
                               const std::string& path) {
        // 第一层：沙箱检查
        if (!Sandbox::is_safe_path(path)) {
            return ToolResult::fail(
                "🛡️  路径遍历防护：禁止 .. 或绝对路径\n"
                "违规路径: " + path
            );
        }
        
        // 第二层：执行操作
        if (operation == "read") {
            return file_read(path);
        }
        // ...
    }
};
```

### 代码工具（注入防护）

```cpp
// code_tool.hpp
#include "sandbox.hpp"

class CodeTool {
public:
    static ToolResult execute(const std::string& code) {
        // 第一层：沙箱检查
        if (!Sandbox::is_safe_code(code)) {
            return ToolResult::fail(
                "🛡️  代码注入防护：包含危险操作\n"
                "违规代码片段已记录"
            );
        }
        
        // 第二层：超时执行
        return execute_with_timeout(code, 5s);
    }
};
```

---

## 第四步：安全测试

### 正常请求

```bash
wscat -c ws://localhost:8080/ws

> 北京天气如何？
[🧠 Processing] "北京天气如何"
  → Tool: get_weather
  → Result: OK
< 北京今天晴天，25°C。

> http_get https://api.github.com/users/github
  → Tool: http_get
  → Result: OK
< {"login": "github", "id": 9919, ...}
```

### 攻击请求（被拦截）

```bash
# 1. SSRF 攻击
> http_get http://localhost/admin
[🧠 Processing] "http_get http://localhost/admin"
  → Tool: http_get
  → Result: FAIL
< 🛡️  SSRF 防护：禁止访问内网或非 HTTP 协议
   违规 URL: http://localhost/admin

# 2. 路径遍历攻击
> file read ../../../etc/passwd
[🧠 Processing] "file read ../../../etc/passwd"
  → Tool: file
  → Result: FAIL
< 🛡️  路径遍历防护：禁止 .. 或绝对路径
   违规路径: ../../../etc/passwd

# 3. 代码注入攻击
> code_execute "import os; os.system('rm -rf /')"
[🧠 Processing] "code_execute ..."
  → Tool: code_execute
  → Result: FAIL
< 🛡️  代码注入防护：包含危险操作
   违规代码片段已记录
```

---

## 本节总结

### 我们实现了什么？

**三层防护体系：**
1. **沙箱层**：统一的 URL/路径/代码检查
2. **工具层**：每个工具调用沙箱检查
3. **执行层**：超时、资源限制

### 代码演进

```
Step 7: 异步工具执行 (600行，10个文件)
   ↓ 演进
Step 8: + 安全沙箱 (700行，14个文件)
   - 新增：sandbox.hpp
   - 新增：http_tool.hpp, file_tool.hpp, code_tool.hpp
   - 演进：tool_executor.hpp（添加安全检查）
```

### 文件演进总结

| 文件 | 状态 | 说明 |
|:---|:---|:---|
| sandbox.hpp | 新增 | 安全沙箱核心 |
| http_tool.hpp | 新增 | HTTP 工具（带 SSRF 防护） |
| file_tool.hpp | 新增 | 文件工具（带路径防护） |
| code_tool.hpp | 新增 | 代码工具（带注入防护） |
| tool_executor.hpp | 演进 | 添加新工具分发 + 安全检查 |
| 其他 10 个文件 | 相同 | 从 Step 7 复用 |

### 安全与便利的平衡

**过度防护的问题：**
- 限制太多 → 工具不好用
- 限制太少 → 有安全风险

**最佳实践：**
- 默认安全（白名单）
- 可配置（根据场景调整）
- 审计日志（记录所有调用）

---

## 附：演进路线图

```
Step 6: 工具系统基础
   ↓
Step 7: + 异步执行
   ↓
Step 8: + 安全沙箱
   ↓
Step 9: + 工具注册表（解决硬编码问题）
```

工具系统逐渐完善：
- **Step 6**：能用（硬编码）
- **Step 7**：好用（异步）
- **Step 8**：安全（沙箱）
- **Step 9**：可扩展（注册表）
