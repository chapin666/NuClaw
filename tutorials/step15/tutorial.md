# Step 15: 性能优化 —— 让 Agent 飞起来

> 目标：实现缓存策略、连接池、性能监控，打造高性能 Agent
> 
003e 难度：⭐⭐⭐⭐ | 代码量：约 1100 行 | 预计学习时间：4-5 小时

---

## 一、为什么需要性能优化？

### 1.1 Step 14 后的性能瓶颈

系统功能已经完善，但生产环境会遇到性能问题：

```
场景 1：1000 用户同时访问
┌─────────────────────────────────────────────┐
│  User 1 ──▶ 创建 HTTP 连接 ──▶ LLM API     │
│  User 2 ──▶ 创建 HTTP 连接 ──▶ LLM API     │
│  User 3 ──▶ 创建 HTTP 连接 ──▶ LLM API     │
│     ...                                     │
│  User 1000 ──▶ 创建 HTTP 连接 ──▶ LLM API  │
└─────────────────────────────────────────────┘
问题：每个请求都新建 TCP 连接，耗时 50-100ms

场景 2：相同问题反复询问
用户 A: "今天北京天气？" ──▶ 调用天气 API ──▶ 2秒
用户 B: "今天北京天气？" ──▶ 调用天气 API ──▶ 2秒
用户 C: "今天北京天气？" ──▶ 调用天气 API ──▶ 2秒
问题：相同请求重复计算，浪费资源

场景 3：LLM Token 消耗
每次对话都携带完整历史：
请求 1: 100 tokens
请求 2: 100 + 200 tokens  
请求 3: 100 + 200 + 300 tokens
...
问题：上下文线性增长，成本和延迟都爆炸
```

### 1.2 性能优化的维度

```
┌─────────────────────────────────────────────────────────────┐
│                     性能优化全景图                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   缓存层     │  │   连接层     │  │   计算层     │      │
│  │              │  │              │  │              │      │
│  │ • 结果缓存   │  │ • HTTP 连接池│  │ • 异步并行   │      │
│  │ • 向量缓存   │  │ • DB 连接池  │  │ • 流式处理   │      │
│  │ • LLM 响应缓存│  │ • 长连接复用 │  │ • 批量处理   │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   存储层     │  │   网络层     │  │   架构层     │      │
│  │              │  │              │  │              │      │
│  │ • 索引优化   │  │ • 压缩传输   │  │ • 水平扩展   │      │
│  │ • 分片策略   │  │ • CDN 加速   │  │ • 负载均衡   │      │
│  │ • 冷热分离   │  │ • 就近部署   │  │ • 无状态设计 │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、缓存策略

### 2.1 缓存的核心思想

**时间换空间，空间换时间**：用内存存储昂贵计算的结果，避免重复计算。

```
┌─────────────────────────────────────────────────────┐
│                   缓存工作流程                        │
├─────────────────────────────────────────────────────┤
│                                                     │
│   请求 ──▶ [检查缓存] ──▶ 命中？                    │
│                │                                    │
│           是 ◄─┴─▶ 否                               │
│            │        │                               │
│            ▼        ▼                               │
│      [返回缓存]  [执行计算]                         │
│                     │                               │
│                     ▼                               │
│               [写入缓存]                            │
│                     │                               │
│                     ▼                               │
│               [返回结果]                            │
│                                                     │
└─────────────────────────────────────────────────────┘
```

**缓存的价值：**
- 减少 LLM API 调用（省钱）
- 减少外部工具调用（省时间）
- 降低数据库查询（省资源）

### 2.2 多级缓存架构

```
请求流量
    │
    ▼
┌─────────────────────────────────────────────────────────┐
│                    L1 - 内存缓存                         │
│              (unordered_map, 10ms 内)                   │
│                   命中？                                 │
└──────────┬──────────────────────────┬───────────────────┘
        命中 │                      未命中
           ▼                         │
      直接返回                        │
                                     ▼
┌─────────────────────────────────────────────────────────┐
│                    L2 - Redis 缓存                       │
│              (分布式, 5-10ms)                           │
│                   命中？                                 │
└──────────┬──────────────────────────┬───────────────────┘
        命中 │                      未命中
           ▼                         │
    回填 L1 并返回                    │
                                     ▼
┌─────────────────────────────────────────────────────────┐
│                  L3 - 实际计算/DB查询                    │
│              (50ms - 2000ms)                            │
└─────────────────────────────────────────────────────────┘
```

### 2.3 缓存键设计

**缓存键是缓存系统的灵魂**，设计不好会导致：
- 缓存穿透（不同请求命中不同键，缓存失效）
- 缓存污染（不同请求命中相同键，返回错误结果）

```cpp
// ❌ 错误示例：简单拼接
cache_key = query + user_id;  // "天气北京" + "user_123"

// ✅ 正确做法：结构化 + 哈希
cache_key = hash({
    "query_hash": hash(normalize(query)),  // 语义归一化
    "context_hash": hash(context),          // 上下文影响结果
    "tool_version": tool_version,           // 工具版本变化结果可能变化
    "cache_scope": "global" | "user" | "session"  // 缓存范围
});

// 示例：天气查询的缓存键
{
    "query_type": "get_weather",
    "location": "北京",
    "date": "2024-03-15",  // 天气按天缓存
    "user_prefs": hash(user.temp_unit)  // 用户偏好影响返回格式
}
// 缓存键: "weather:get_weather:北京:2024-03-15:celsius"
```

### 2.4 缓存实现

```cpp
#include <cache>  // C++17 标准库，或自定义实现

// 带过期时间的缓存项
template<typename T>
struct CacheEntry {
    T value;
    std::chrono::steady_clock::time_point expires_at;
    int64_t hit_count = 0;  // 统计命中率
};

class LRUCache {
public:
    using Key = std::string;
    using Clock = std::chrono::steady_clock;
    
    struct Entry {
        std::string value;
        Clock::time_point expires_at;
        std::list<Key>::iterator lru_iter;
    };
    
    LRUCache(size_t max_size, std::chrono::seconds default_ttl)
        : max_size_(max_size), default_ttl_(default_ttl) {}
    
    // 获取缓存
    std::optional<std::string> get(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            return std::nullopt;
        }
        
        // 检查过期
        if (Clock::now() > it->second.expires_at) {
            evict(it);
            return std::nullopt;
        }
        
        // 更新 LRU
        touch(it);
        
        return it->second.value;
    }
    
    // 设置缓存
    void set(const Key& key, const std::string& value, 
             std::optional<std::chrono::seconds> ttl = std::nullopt) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto expiration = Clock::now() + (ttl.value_or(default_ttl_));
        
        // 如果已存在，更新
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            it->second.value = value;
            it->second.expires_at = expiration;
            touch(it);
            return;
        }
        
        // 淘汰最久未使用
        while (cache_.size() >= max_size_) {
            evict_oldest();
        }
        
        // 插入新项
        lru_list_.push_front(key);
        cache_[key] = {value, expiration, lru_list_.begin()};
    }
    
    // 删除缓存
    void del(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            evict(it);
        }
    }
    
    // 统计信息
    struct Stats {
        size_t size;
        size_t max_size;
        size_t hit_count;
        size_t miss_count;
        double hit_rate() const {
            auto total = hit_count + miss_count;
            return total > 0 ? (double)hit_count / total : 0;
        }
    };
    
    Stats get_stats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return {cache_.size(), max_size_, hit_count_, miss_count_};
    }

private:
    void touch(typename std::unordered_map<Key, Entry>::iterator it) {
        // 移到链表头部（最近使用）
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second.lru_iter);
    }
    
    void evict(typename std::unordered_map<Key, Entry>::iterator it) {
        lru_list_.erase(it->second.lru_iter);
        cache_.erase(it);
    }
    
    void evict_oldest() {
        if (!lru_list_.empty()) {
            auto it = cache_.find(lru_list_.back());
            if (it != cache_.end()) {
                evict(it);
            }
        }
    }

    size_t max_size_;
    std::chrono::seconds default_ttl_;
    
    std::unordered_map<Key, Entry> cache_;
    std::list<Key> lru_list_;  // 维护 LRU 顺序
    mutable std::mutex mutex_;
    
    mutable size_t hit_count_ = 0;
    mutable size_t miss_count_ = 0;
};
```

### 2.5 Agent 中的缓存应用

```cpp
class CachingAgent {
public:
    CachingAgent(LLMClient& llm) 
        : llm_(llm),
          // 缓存配置：最大 1000 条，默认 5 分钟过期
          response_cache_(1000, std::chrono::minutes(5)),
          embedding_cache_(5000, std::chrono::hours(1)) {}
    
    void process(const std::string& user_input,
                 const Context& ctx,
                 std::function<void(const std::string&)> callback) {
        
        // 1. 生成缓存键（考虑用户上下文）
        std::string cache_key = generate_cache_key(user_input, ctx);
        
        // 2. 检查缓存
        if (auto cached = response_cache_.get(cache_key)) {
            logger::info("Cache hit for key: {}", cache_key);
            callback(*cached);
            return;
        }
        
        // 3. 未命中，执行实际处理
        do_process(user_input, ctx, 
            [this, cache_key, callback](const std::string& response) {
                // 4. 写入缓存
                response_cache_.set(cache_key, response);
                callback(response);
            }
        );
    }
    
    // Embedding 缓存（昂贵的向量化操作）
    void get_embedding(const std::string& text,
                       std::function<void(const std::vector<float>&)> callback) {
        
        std::string key = "emb:" + std::to_string(std::hash<std::string>{}(text));
        
        if (auto cached = embedding_cache_.get(key)) {
            // 解析缓存的向量
            callback(parse_embedding(*cached));
            return;
        }
        
        // 调用 Embedding API
        llm_.embed(text, [this, key, callback](auto embedding) {
            // 序列化并缓存
            embedding_cache_.set(key, serialize_embedding(embedding));
            callback(embedding);
        });
    }

private:
    std::string generate_cache_key(const std::string& input, const Context& ctx) {
        // 关键：哪些因素影响响应？
        json key_data = {
            {"query", normalize(input)},
            {"user_id", ctx.user_id},
            {"session_summary", ctx.summary_hash},  // 摘要而非完整历史
            {"tools_available", ctx.tool_versions}
        };
        return "resp:" + sha256(key_data.dump());
    }
    
    LLMClient& llm_;
    LRUCache response_cache_;
    LRUCache embedding_cache_;
};
```

---

## 三、连接池

### 3.1 为什么需要连接池？

**TCP 连接的成本：**
```
新建连接流程：
1. DNS 解析 ─────────────────────▶ 10-50ms
2. TCP 三次握手 ─────────────────▶ RTT (10-100ms)
3. TLS 握手（HTTPS） ────────────▶ 2-RTT (20-200ms)
4. HTTP 请求/响应 ───────────────▶ 业务耗时
5. 连接关闭（四次挥手） ─────────▶ RTT

总耗时：50-500ms

连接池复用：
1. 从池中取连接 ─────────────────▶ 1μs
2. HTTP 请求/响应 ───────────────▶ 业务耗时
3. 归还连接到池 ─────────────────▶ 1μs

性能提升：100-1000 倍
```

### 3.2 HTTP 连接池实现

```cpp
class HttpConnectionPool {
public:
    struct Connection {
        beast::tcp_stream stream;
        Clock::time_point last_used;
        bool in_use = false;
    };
    
    HttpConnectionPool(asio::io_context& io,
                       const std::string& host,
                       size_t max_size = 10)
        : io_(io), host_(host), max_size_(max_size) {}
    
    // 获取连接
    void acquire(std::function<void(std::shared_ptr<Connection>)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 1. 查找空闲连接
        for (auto& conn : pool_) {
            if (!conn->in_use && is_valid(conn)) {
                conn->in_use = true;
                callback(conn);
                return;
            }
        }
        
        // 2. 池未满，创建新连接
        if (pool_.size() < max_size_) {
            auto conn = std::make_shared<Connection>(io_);
            conn->in_use = true;
            pool_.push_back(conn);
            
            // 异步连接
            do_connect(conn, callback);
            return;
        }
        
        // 3. 池已满，加入等待队列
        waiting_queue_.push(callback);
    }
    
    // 归还连接
    void release(std::shared_ptr<Connection> conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        conn->in_use = false;
        conn->last_used = Clock::now();
        
        // 如果有等待的请求，直接分配
        if (!waiting_queue_.empty()) {
            auto callback = waiting_queue_.front();
            waiting_queue_.pop();
            conn->in_use = true;
            callback(conn);
        }
    }

private:
    bool is_valid(std::shared_ptr<Connection> conn) {
        // 检查连接是否存活（超过 30 秒未用可能已超时）
        auto idle_time = Clock::now() - conn->last_used;
        if (idle_time > std::chrono::seconds(30)) {
            return false;
        }
        return conn->stream.socket().is_open();
    }
    
    void do_connect(std::shared_ptr<Connection> conn,
                    std::function<void(std::shared_ptr<Connection>)> callback) {
        auto resolver = std::make_shared<tcp::resolver>(io_);
        
        resolver->async_resolve(
            host_, "443",
            [this, conn, resolver, callback](auto ec, auto results) {
                if (ec) {
                    callback(nullptr);
                    return;
                }
                
                conn->stream.async_connect(
                    results,
                    [conn, callback](auto ec, auto) {
                        if (ec) {
                            callback(nullptr);
                        } else {
                            callback(conn);
                        }
                    }
                );
            }
        );
    }

    asio::io_context& io_;
    std::string host_;
    size_t max_size_;
    
    std::vector<std::shared_ptr<Connection>> pool_;
    std::queue<std::function<void(std::shared_ptr<Connection>)>> waiting_queue_;
    std::mutex mutex_;
};
```

### 3.3 连接池在 LLM Client 中的应用

```cpp
class PooledLLMClient {
public:
    PooledLLMClient(asio::io_context& io, const Config& config)
        : pool_(io, config.api_endpoint, config.pool_size) {}
    
    void chat(const std::vector<Message>& messages,
              std::function<void(Response)> callback) {
        
        // 从连接池获取连接
        pool_.acquire([this, messages, callback](auto conn) {
            if (!conn) {
                callback({.error = "No connection available"});
                return;
            }
            
            // 发送请求
            send_request(conn, messages, 
                [this, conn, callback](Response resp) {
                    // 归还连接到池
                    pool_.release(conn);
                    callback(resp);
                }
            );
        });
    }

private:
    HttpConnectionPool pool_;
};
```

---

## 四、性能监控与调优

### 4.1 关键性能指标

```cpp
struct PerformanceMetrics {
    // 延迟指标（毫秒）
    struct Latency {
        double p50;   // 中位数
        double p95;   // 95 分位（95% 请求低于此值）
        double p99;   // 99 分位（长尾延迟）
    };
    
    // 吞吐量
    double requests_per_second;
    
    // 资源使用
    struct Resources {
        double cpu_percent;
        size_t memory_mb;
        size_t connections_active;
        size_t cache_hit_rate;
    };
    
    // LLM 特定指标
    struct LLM {
        double tokens_per_second;
        double cost_per_request;
        size_t context_window_usage;
    };
};
```

### 4.2 性能剖析示例

```cpp
class PerformanceProfiler {
public:
    // 自动计时器，RAII 模式
    class Timer {
    public:
        Timer(PerformanceProfiler& profiler, const std::string& operation)
            : profiler_(profiler), operation_(operation),
              start_(std::chrono::steady_clock::now()) {}
        
        ~Timer() {
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - start_
            ).count();
            profiler_.record(operation_, elapsed);
        }
    
    private:
        PerformanceProfiler& profiler_;
        std::string operation_;
        std::chrono::steady_clock::time_point start_;
    };
    
    void record(const std::string& operation, int64_t microseconds) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_[operation].push_back(microseconds);
    }
    
    // 生成性能报告
    json report() {
        std::lock_guard<std::mutex> lock(mutex_);
        json result;
        
        for (const auto& [op, times] : data_) {
            auto sorted = times;
            std::sort(sorted.begin(), sorted.end());
            
            result[op] = {
                {"count", sorted.size()},
                {"p50", percentile(sorted, 0.5)},
                {"p95", percentile(sorted, 0.95)},
                {"p99", percentile(sorted, 0.99)},
                {"avg", average(sorted)}
            };
        }
        
        return result;
    }

private:
    int64_t percentile(const std::vector<int64_t>& sorted, double p) {
        size_t idx = static_cast<size_t>(sorted.size() * p);
        return sorted[std::min(idx, sorted.size() - 1)];
    }
    
    std::unordered_map<std::string, std::vector<int64_t>> data_;
    std::mutex mutex_;
};

// 使用示例
void process_request(Request& req) {
    PerformanceProfiler::Timer timer(g_profiler, "process_request");
    
    {
        PerformanceProfiler::Timer timer2(g_profiler, "llm_call");
        auto response = llm_.chat(req.messages);
    }
    
    {
        PerformanceProfiler::Timer timer3(g_profiler, "tool_execution");
        execute_tools(response.tool_calls);
    }
}
```

---

## 五、本章小结

**核心收获：**

1. **缓存策略**：
   - 多级缓存架构
   - LRU 淘汰算法
   - 缓存键设计（考虑上下文）
   - Embedding 结果缓存

2. **连接池**：
   - 复用 TCP 连接，避免握手开销
   - 池大小控制
   - 等待队列机制

3. **性能监控**：
   - 延迟分位数（P50/P95/P99）
   - 自动性能剖析
   - 资源使用监控

**性能优化原则：**
- 先测量，再优化（无数据不优化）
- 缓存是万金油，但要注意一致性
- 连接池是 I/O 密集型应用的标配

---

## 六、引出的问题

### 6.1 分布式扩展问题

单机的优化有极限，用户量增加后需要水平扩展：

```
单机架构：
┌─────────────────┐
│   Agent Server  │
│  ┌───────────┐  │
│  │  Cache    │  │
│  │  Session  │  │  容量有限
│  │  Tools    │  │
│  └───────────┘  │
└─────────────────┘

分布式架构：
┌─────────┐ ┌─────────┐ ┌─────────┐
│ Agent 1 │ │ Agent 2 │ │ Agent 3 │
└────┬────┘ └────┬────┘ └────┬────┘
     └───────────┼───────────┘
                 │
        ┌────────┴────────┐
        │  Redis Cluster  │
        │  (共享 Session) │
        └─────────────────┘
```

**需要：** 分布式 Session、负载均衡、数据一致性。

---

**下一章预告（Step 16）：**

我们将总结整个 NuClaw 架构，并展望 AI Agent 的未来发展方向：
- 完整架构回顾
- 分布式扩展设计
- AI Agent 技术趋势
- 从框架到产品的演进路径

性能优化已经完成，是时候回望全局，规划未来了。
