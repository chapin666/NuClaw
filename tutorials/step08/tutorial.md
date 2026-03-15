# Step 8: 安全沙箱 —— Agent 的"安全护栏"

> 目标：实现 SSRF 防护、路径限制和审计日志，确保工具执行安全
> 
003e 难度：⭐⭐⭐⭐ | 代码量：约 700 行 | 预计学习时间：3-4 小时

---

## 一、为什么需要安全沙箱？

### 1.1 Step 7 的安全隐患

Step 7 实现了异步工具执行，但存在严重安全隐患：

```cpp
// 危险！可能遭受 SSRF 攻击
void execute_async(const json& args, ...) {
    std::string url = args["url"];  // 用户可控！
    http_client_.get(url, ...);     // 可以访问任意地址！
}

// 攻击示例：
args: {"url": "http://localhost:8080/admin/delete-all"}  // 攻击内网服务
args: {"url": "file:///etc/passwd"}                       // 读取系统文件
args: {"url": "http://169.254.169.254/latest/meta-data/"} // 读取云元数据
```

### 1.2 常见攻击类型

| 攻击类型 | 说明 | 示例 |
|:---|:---|:---|
| **SSRF** | 服务器端请求伪造，访问内网资源 | `http://localhost/secret` |
| **路径遍历** | 访问非授权目录 | `../../../etc/passwd` |
| **命令注入** | 执行任意系统命令 | `; rm -rf /` |
| **资源耗尽** | 消耗过多系统资源 | 超大文件下载、无限循环 |

### 1.3 安全沙箱的目标

```
┌─────────────────────────────────────────────────────────────┐
│                      安全沙箱（Sandbox）                      │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  输入 ──▶ [安全策略检查] ──▶ [工具执行] ──▶ [审计记录]        │
│              │                     │                       │
│              ▼                     ▼                       │
│         ┌──────────┐         ┌──────────┐                 │
│         │  URL 白名单 │         │ 资源限制  │                 │
│         │  IP 黑名单 │         │ 超时控制  │                 │
│         │  路径限制  │         │ 权限控制  │                 │
│         └──────────┘         └──────────┘                 │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、SSRF 防护

### 2.1 什么是 SSRF？

**SSRF（Server-Side Request Forgery）**：攻击者通过服务器发起恶意请求，访问内网资源。

```
攻击流程：

攻击者 ──▶ 公网服务器 ──▶ 内网服务
         (你的 Agent)     (数据库、管理接口、云元数据)
         
攻击者无法直接访问内网，但可以通过你的服务器"借刀杀人"
```

**危害：**
- 访问内网管理接口
- 读取云服务商元数据（获取密钥）
- 攻击内部服务（Redis、Elasticsearch）

### 2.2 防护策略

#### 策略 1：URL 白名单

```cpp
class SSRFFilter {
public:
    // 允许的域名列表
    void add_allowed_domain(const std::string& domain) {
        allowed_domains_.insert(domain);
    }
    
    // 验证 URL 是否安全
    bool is_url_safe(const std::string& url) {
        // 解析 URL
        auto parsed = parse_url(url);
        
        // 1. 检查协议
        if (parsed.protocol != "http" && parsed.protocol != "https") {
            return false;  // 禁止 file://, ftp:// 等
        }
        
        // 2. 检查域名
        if (!allowed_domains_.empty()) {
            if (!allowed_domains_.count(parsed.host)) {
                return false;
            }
        }
        
        // 3. 检查 IP 是否是内网
        if (is_private_ip(parsed.host)) {
            return false;
        }
        
        // 4. 检查端口（只允许 80/443）
        if (parsed.port != 80 && parsed.port != 443) {
            return false;
        }
        
        return true;
    }

private:
    bool is_private_ip(const std::string& host) {
        // 解析 IP 地址
        asio::ip::address addr;
        try {
            addr = asio::ip::make_address(host);
        } catch (...) {
            // 不是 IP，可能是域名，需要进一步解析检查
            return false;
        }
        
        // 检查是否是内网 IP
        if (addr.is_v4()) {
            auto v4 = addr.to_v4();
            // 10.0.0.0/8
            if ((v4.to_ulong() & 0xFF000000) == 0x0A000000) return true;
            // 172.16.0.0/12
            if ((v4.to_ulong() & 0xFFF00000) == 0xAC100000) return true;
            // 192.168.0.0/16
            if ((v4.to_ulong() & 0xFFFF0000) == 0xC0A80000) return true;
            // 127.0.0.0/8 (localhost)
            if ((v4.to_ulong() & 0xFF000000) == 0x7F000000) return true;
        }
        
        return false;
    }

    std::set<std::string> allowed_domains_;
};
```

#### 策略 2：DNS 解析检查

```cpp
bool is_safe_host(const std::string& host) {
    // 先检查是否是 IP
    asio::ip::address addr;
    try {
        addr = asio::ip::make_address(host);
        // 直接传入 IP，检查是否是私网
        return !is_private_ip(addr);
    } catch (...) {
        // 是域名，需要解析
    }
    
    // 解析域名
    tcp::resolver resolver(io_);
    auto results = resolver.resolve(host, "80");
    
    // 检查解析后的所有 IP
    for (const auto& entry : results) {
        if (is_private_ip(entry.endpoint().address())) {
            return false;  // 解析到内网 IP，拒绝
        }
    }
    
    return true;
}
```

**为什么需要 DNS 检查？**

```
攻击者注册域名：evil.com
DNS 配置：evil.com 指向 192.168.1.1（内网）

请求：http://evil.com
通过域名白名单检查（evil.com 不在黑名单）
但实际连接的是 192.168.1.1！

解决方案：在连接前解析域名，检查解析结果
```

### 2.3 集成到 HTTP 客户端

```cpp
class SecureHttpClient {
public:
    SecureHttpClient(asio::io_context& io) : io_(io) {
        // 配置 SSRF 过滤器
        ssrf_filter_.add_allowed_domain("api.openweathermap.org");
        ssrf_filter_.add_allowed_domain("api.github.com");
        // ...
    }
    
    void get(const std::string& url,
             std::chrono::milliseconds timeout,
             std::function<void(HttpResponse)> callback) {
        
        // 安全检查时
        if (!ssrf_filter_.is_url_safe(url)) {
            callback(HttpResponse{
                .success = false,
                .error = "SSRF: URL not allowed - " + url
            });
            return;
        }
        
        // 继续执行 HTTP 请求...
        do_get(url, timeout, callback);
    }

private:
    asio::io_context& io_;
    SSRFFilter ssrf_filter_;
};
```

---

## 三、路径安全

### 3.1 路径遍历攻击

```cpp
// 危险代码
void read_file(const std::string& path) {
    std::ifstream file(base_path_ + "/" + path);  // 用户可控！
    // ...
}

// 攻击：
path: "../../../etc/passwd"
实际访问：/app/data/../../../etc/passwd = /etc/passwd
```

### 3.2 路径规范化与限制

```cpp
class PathValidator {
public:
    PathValidator(const std::string& base_path) 
        : base_path_(fs::absolute(base_path)) {}
    
    // 验证并规范化路径
    std::optional<std::string> validate(const std::string& user_path) {
        // 1. 拼接路径
        fs::path full_path = base_path_ / user_path;
        
        // 2. 规范化（解析 .. 和 .）
        full_path = fs::weakly_canonical(full_path);
        
        // 3. 检查是否仍在 base_path 下
        auto relative = fs::relative(full_path, base_path_);
        
        // 如果 relative 以 .. 开头，说明越界
        if (relative.string().find("..") == 0) {
            return std::nullopt;  // 非法路径
        }
        
        // 4. 检查文件扩展名白名单
        if (!is_allowed_extension(full_path.extension())) {
            return std::nullopt;
        }
        
        return full_path.string();
    }

private:
    bool is_allowed_extension(const std::string& ext) {
        static const std::set<std::string> allowed = {
            ".txt", ".json", ".xml", ".md"
        };
        return allowed.count(ext);
    }

    fs::path base_path_;
};
```

---

## 四、资源限制

### 4.1 资源限制策略

```cpp
struct ResourceLimits {
    size_t max_response_size = 10 * 1024 * 1024;     // 10 MB
    size_t max_request_rate = 100;                    // 每秒请求数
    std::chrono::milliseconds max_execution_time = std::chrono::seconds(30);
    size_t max_concurrent_executions = 10;
};

class ResourceLimiter {
public:
    bool check_and_acquire(const std::string& tool_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 检查并发数
        if (active_count_ >= limits_.max_concurrent_executions) {
            return false;
        }
        
        // 检查速率限制
        auto now = std::chrono::steady_clock::now();
        cleanup_old_requests(now);
        
        if (request_times_.size() >= limits_.max_request_rate) {
            return false;
        }
        
        request_times_.push_back(now);
        active_count_++;
        return true;
    }
    
    void release() {
        std::lock_guard<std::mutex> lock(mutex_);
        active_count_--;
    }

private:
    void cleanup_old_requests(auto now) {
        auto cutoff = now - std::chrono::seconds(1);
        request_times_.erase(
            std::remove_if(request_times_.begin(), request_times_.end(),
                [cutoff](auto t) { return t < cutoff; }),
            request_times_.end()
        );
    }

    ResourceLimits limits_;
    std::vector<decltype(std::chrono::steady_clock::now())> request_times_;
    size_t active_count_ = 0;
    std::mutex mutex_;
};
```

### 4.2 响应大小限制

```cpp
void async_read_with_limit(tcp::socket& socket,
                           size_t max_size,
                           std::function<void(std::string)> callback) {
    auto buffer = std::make_shared<std::string>();
    buffer->reserve(8192);
    
    auto read_chunk = [socket_ptr, buffer, max_size, callback](auto self, 
                                                               error_code ec, 
                                                               size_t n) mutable {
        if (ec) {
            callback(*buffer);
            return;
        }
        
        if (buffer->size() > max_size) {
            callback("ERROR: Response too large");
            return;
        }
        
        // 继续读取...
        socket.async_read_some(..., 
            [self](auto... args) { self(self, args...); });
    };
    
    // 开始读取
    read_chunk(read_chunk, error_code{}, 0);
}
```

---

## 五、审计日志

### 5.1 审计的重要性

安全事件发生后，需要追溯：
- 谁调用了什么工具？
- 传入了什么参数？
- 返回了什么结果？
- 执行花了多长时间？

### 5.2 审计日志实现

```cpp
struct AuditLog {
    std::string timestamp;
    std::string session_id;
    std::string user_id;
    std::string tool_name;
    json arguments;
    json result;
    int64_t execution_time_ms;
    bool success;
    std::string error_message;
};

class AuditLogger {
public:
    AuditLogger(const std::string& log_dir) : log_dir_(log_dir) {
        // 按日期创建日志文件
        rotate_log();
    }
    
    void log(const AuditLog& entry) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        json log_entry = {
            {"timestamp", entry.timestamp},
            {"session_id", entry.session_id},
            {"user_id", entry.user_id},
            {"tool_name", entry.tool_name},
            {"arguments", entry.arguments},
            {"result", entry.result},
            {"execution_time_ms", entry.execution_time_ms},
            {"success", entry.success}
        };
        
        if (!entry.success) {
            log_entry["error"] = entry.error_message;
        }
        
        // 写入日志文件
        file_ << log_entry.dump() << "\n";
        file_.flush();
    }
    
    // 查询日志（安全审计用）
    std::vector<AuditLog> query(const std::string& session_id,
                                  const std::string& start_time,
                                  const std::string& end_time);

private:
    void rotate_log() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << log_dir_ << "/audit_" << std::put_time(std::localtime(&time), "%Y%m%d") << ".log";
        
        file_.open(ss.str(), std::ios::app);
    }

    std::string log_dir_;
    std::ofstream file_;
    std::mutex mutex_;
};
```

### 5.3 集成到工具执行

```cpp
void SecureToolManager::execute_async(
    const std::string& tool_name,
    const json& arguments,
    const ToolContext& context,
    std::function<void(ToolResult)> callback) {
    
    auto start = std::chrono::steady_clock::now();
    auto start_str = get_current_timestamp();
    
    // 1. 安全检查
    if (!security_checker_.check(tool_name, arguments)) {
        AuditLog log{
            .timestamp = start_str,
            .session_id = context.session_id,
            .user_id = context.user_id,
            .tool_name = tool_name,
            .arguments = arguments,
            .success = false,
            .error_message = "Security check failed"
        };
        audit_logger_.log(log);
        
        callback(ToolResult{.success = false, .error_message = "Security check failed"});
        return;
    }
    
    // 2. 资源限制检查
    if (!resource_limiter_.check_and_acquire(tool_name)) {
        AuditLog log{
            .timestamp = start_str,
            .session_id = context.session_id,
            .tool_name = tool_name,
            .success = false,
            .error_message = "Resource limit exceeded"
        };
        audit_logger_.log(log);
        
        callback(ToolResult{.success = false, .error_message = "Too many requests"});
        return;
    }
    
    // 3. 执行工具
    auto tool = get_tool(tool_name);
    tool->execute_async(arguments, context, timeout,
        [this, start, start_str, tool_name, arguments, context, callback](ToolResult result) {
            resource_limiter_.release();
            
            auto end = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<
                std::chrono::milliseconds>(end - start).count();
            
            // 4. 记录审计日志
            AuditLog log{
                .timestamp = start_str,
                .session_id = context.session_id,
                .user_id = context.user_id,
                .tool_name = tool_name,
                .arguments = arguments,
                .result = result.data,
                .execution_time_ms = elapsed,
                .success = result.success,
                .error_message = result.error_message
            };
            audit_logger_.log(log);
            
            callback(result);
        }
    );
}
```

---

## 六、本章小结

**核心收获：**

1. **SSRF 防护**：
   - URL 白名单/黑名单
   - IP 地址检查（内网 IP 拒绝）
   - DNS 解析验证

2. **路径安全**：
   - 路径规范化
   - 目录限制（chroot 思想）
   - 扩展名白名单

3. **资源限制**：
   - 响应大小限制
   - 并发数限制
   - 速率限制

4. **审计日志**：
   - 全量记录工具调用
   - 便于安全追溯

---

## 七、引出的问题

### 7.1 工具管理问题

目前工具还是硬编码注册：

```cpp
void register_tools() {
    tool_manager_.register_tool(std::make_shared<WeatherTool>());
    tool_manager_.register_tool(std::make_shared<CalcTool>());
    // 每加一个工具都要改代码
}
```

**需要：** 动态工具注册、依赖注入、配置文件驱动。

### 7.2 复杂工具依赖

工具之间可能有依赖关系：

```
TravelPlanningTool
  ├── 依赖 WeatherTool
  ├── 依赖 HotelSearchTool
  └── 依赖 FlightSearchTool
```

**需要：** 依赖注入框架、自动解析依赖。

---

**下一章预告（Step 9）：**

我们将实现**工具注册表**：
- 注册表模式（Registry Pattern）
- 依赖注入（Dependency Injection）
- 配置文件驱动工具注册
- 支持动态加载工具

安全防护已经就位，接下来要让工具管理更灵活、更解耦。
