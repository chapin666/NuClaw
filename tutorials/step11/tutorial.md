# Step 11: 数据持久化

> 目标：实现数据库存储，支持会话和聊天记录持久化
> 
> 难度：⭐⭐⭐ | 代码量：约 900 行 | 预计学习时间：3-4 小时

---

## 一、问题引入

### 1.1 Step 10 的问题

目前所有数据都存储在内存中：
```
问题 1: 服务重启后所有会话丢失
  - 用户正在聊天
  - 服务器重启
  - 所有会话历史清空 💥

问题 2: 无法支持多实例部署
  - 实例 A 的用户会话
  - 实例 B 无法访问
  - 负载均衡后用户会话错乱

问题 3: 数据无法分析
  - 无法查看历史聊天记录
  - 无法统计用户使用情况
  - 无法做数据挖掘
```

### 1.2 本章目标

1. **数据库存储**：SQLite/PostgreSQL
2. **会话持久化**：重启后恢复
3. **聊天记录存储**：历史记录查询
4. **迁移支持**：数据库版本管理

---

## 二、核心概念

### 2.1 数据库选择

| 数据库 | 优点 | 缺点 | 适用场景 |
|:---|:---|:---|:---|
| SQLite | 轻量、无需配置 | 不适合高并发 | 开发/单实例 |
| PostgreSQL | 功能强大、可靠 | 需要部署 | 生产环境 |
| MySQL | 流行、生态好 | 配置复杂 | 已有 MySQL 环境 |
| Redis | 高性能 | 内存存储 | 缓存/会话 |

**本章使用 SQLite**（简单，生产可替换为 PostgreSQL）

### 2.2 数据模型设计

```sql
-- 会话表
CREATE TABLE sessions (
    session_id TEXT PRIMARY KEY,
    user_id TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_activity TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    metadata TEXT  -- JSON 格式
);

-- 聊天记录表
CREATE TABLE chat_messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id TEXT NOT NULL,
    role TEXT NOT NULL,  -- user/assistant/system/tool
    content TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (session_id) REFERENCES sessions(session_id)
);

-- 长期记忆表
CREATE TABLE long_term_memory (
    id TEXT PRIMARY KEY,
    session_id TEXT NOT NULL,
    content TEXT NOT NULL,
    embedding BLOB,  -- 二进制存储向量
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (session_id) REFERENCES sessions(session_id)
);
```

---

## 三、代码结构详解

### 3.1 数据库连接管理

```cpp
#include <sqlite3.h>

class Database {
public:
    Database(const std::string& db_path) {
        int rc = sqlite3_open(db_path.c_str(), &db_);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Cannot open database: " + 
                                    std::string(sqlite3_errmsg(db_)));
        }
        
        // 启用外键约束
        execute("PRAGMA foreign_keys = ON;");
        
        // 初始化表结构
        init_tables();
    }
    
    ~Database() {
        sqlite3_close(db_);
    }
    
    // 执行 SQL（无返回）
    void execute(const std::string& sql) {
        char* err_msg = nullptr;
        int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
        
        if (rc != SQLITE_OK) {
            std::string error(err_msg);
            sqlite3_free(err_msg);
            throw std::runtime_error("SQL error: " + error);
        }
    }
    
    // 预编译语句执行
    template<typename... Args>
    void execute_prepared(const std::string& sql, Args... args) {
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        
        bind_params(stmt, 1, args...);
        
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

private:
    void init_tables() {
        execute(R"(
            CREATE TABLE IF NOT EXISTS sessions (
                session_id TEXT PRIMARY KEY,
                user_id TEXT NOT NULL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                last_activity TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                metadata TEXT
            );
            
            CREATE TABLE IF NOT EXISTS chat_messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                session_id TEXT NOT NULL,
                role TEXT NOT NULL,
                content TEXT,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (session_id) REFERENCES sessions(session_id)
            );
            
            CREATE INDEX IF NOT EXISTS idx_messages_session 
            ON chat_messages(session_id);
        )");
    }
    
    template<typename T, typename... Rest>
    void bind_params(sqlite3_stmt* stmt, int index, T value, Rest... rest) {
        if constexpr (std::is_same_v<T, int>) {
            sqlite3_bind_int(stmt, index, value);
        } else if constexpr (std::is_same_v<T, std::string>) {
            sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
        }
        bind_params(stmt, index + 1, rest...);
    }
    
    void bind_params(sqlite3_stmt* stmt, int index) {}
    
    sqlite3* db_;
};
```

### 3.2 持久化 SessionManager

```cpp
class PersistentSessionManager : public SessionManager {
public:
    PersistentSessionManager(Database& db) : db_(db) {
        // 启动时加载所有未过期会话
        load_sessions();
    }
    
    std::string create_session(const std::string& user_id) override {
        std::string session_id = generate_uuid();
        
        // 插入数据库
        db_.execute_prepared(
            "INSERT INTO sessions (session_id, user_id, metadata) VALUES (?, ?, ?);",
            session_id,
            user_id,
            "{}"
        );
        
        // 添加到内存
        SessionData data;
        data.session_id = session_id;
        data.user_id = user_id;
        data.touch();
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            sessions_[session_id] = std::move(data);
        }
        
        return session_id;
    }
    
    void update_history(const std::string& session_id,
                       const std::vector<json>& history) override {
        // 更新内存
        SessionManager::update_history(session_id, history);
        
        // 保存到数据库
        save_messages(session_id, history);
    }
    
    void save_message(const std::string& session_id,
                      const std::string& role,
                      const std::string& content) {
        db_.execute_prepared(
            "INSERT INTO chat_messages (session_id, role, content) VALUES (?, ?, ?);",
            session_id,
            role,
            content
        );
        
        // 更新最后活动时间
        db_.execute_prepared(
            "UPDATE sessions SET last_activity = CURRENT_TIMESTAMP WHERE session_id = ?;",
            session_id
        );
    }

private:
    void load_sessions() {
        // 查询数据库加载未过期会话
        sqlite3_stmt* stmt;
        const char* sql = 
            "SELECT session_id, user_id, metadata FROM sessions "
            "WHERE last_activity > datetime('now', '-1 day');";
        
        sqlite3_prepare_v2(db_.get_handle(), sql, -1, &stmt, nullptr);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            SessionData data;
            data.session_id = reinterpret_cast<const char*>(
                sqlite3_column_text(stmt, 0));
            data.user_id = reinterpret_cast<const char*>(
                sqlite3_column_text(stmt, 1));
            data.touch();
            
            // 加载历史消息
            data.history = load_messages(data.session_id);
            
            sessions_[data.session_id] = std::move(data);
        }
        
        sqlite3_finalize(stmt);
    }
    
    std::vector<json> load_messages(const std::string& session_id) {
        std::vector<json> messages;
        
        sqlite3_stmt* stmt;
        const char* sql = 
            "SELECT role, content FROM chat_messages "
            "WHERE session_id = ? ORDER BY created_at ASC;";
        
        sqlite3_prepare_v2(db_.get_handle(), sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, session_id.c_str(), -1, SQLITE_STATIC);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            messages.push_back({
                {"role", reinterpret_cast<const char*>(
                    sqlite3_column_text(stmt, 0))},
                {"content", reinterpret_cast<const char*>(
                    sqlite3_column_text(stmt, 1))}
            });
        }
        
        sqlite3_finalize(stmt);
        return messages;
    }
    
    void save_messages(const std::string& session_id,
                       const std::vector<json>& history) {
        // 简化：先删除旧记录，再插入新记录
        db_.execute_prepared(
            "DELETE FROM chat_messages WHERE session_id = ?;",
            session_id
        );
        
        for (const auto& msg : history) {
            save_message(
                session_id,
                msg.value("role", ""),
                msg.value("content", "")
            );
        }
    }
    
    Database& db_;
};
```

### 3.3 向量存储持久化

```cpp
class PersistentVectorStore : public VectorStore {
public:
    PersistentVectorStore(Database& db) : db_(db) {
        init_table();
        load_from_db();
    }
    
    void add(const MemoryItem& item) override {
        // 先添加到内存
        VectorStore::add(item);
        
        // 保存到数据库
        save_to_db(item);
    }

private:
    void init_table() {
        db_.execute(R"(
            CREATE TABLE IF NOT EXISTS vector_memory (
                id TEXT PRIMARY KEY,
                session_id TEXT NOT NULL,
                content TEXT NOT NULL,
                embedding BLOB,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            );
        )");
    }
    
    void save_to_db(const MemoryItem& item) {
        // 将 vector<float> 序列化为二进制
        std::string blob_data;
        blob_data.resize(item.embedding.size() * sizeof(float));
        memcpy(&blob_data[0], item.embedding.data(), blob_data.size());
        
        sqlite3_stmt* stmt;
        const char* sql = 
            "INSERT OR REPLACE INTO vector_memory "
            "(id, session_id, content, embedding) VALUES (?, ?, ?, ?);";
        
        sqlite3_prepare_v2(db_.get_handle(), sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, item.id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, item.session_id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, item.content.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 4, blob_data.data(), blob_data.size(), SQLITE_STATIC);
        
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    void load_from_db() {
        sqlite3_stmt* stmt;
        const char* sql = "SELECT * FROM vector_memory;";
        
        sqlite3_prepare_v2(db_.get_handle(), sql, -1, &stmt, nullptr);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            MemoryItem item;
            item.id = reinterpret_cast<const char*>(
                sqlite3_column_text(stmt, 0));
            item.session_id = reinterpret_cast<const char*>(
                sqlite3_column_text(stmt, 1));
            item.content = reinterpret_cast<const char*>(
                sqlite3_column_text(stmt, 2));
            
            // 反序列化 embedding
            const void* blob = sqlite3_column_blob(stmt, 3);
            int blob_size = sqlite3_column_bytes(stmt, 3);
            int num_floats = blob_size / sizeof(float);
            
            item.embedding.resize(num_floats);
            memcpy(item.embedding.data(), blob, blob_size);
            
            // 添加到内存
            VectorStore::add(item);
        }
        
        sqlite3_finalize(stmt);
    }
    
    Database& db_;
};
```

---

## 四、数据库迁移

```cpp
class MigrationManager {
public:
    MigrationManager(Database& db) : db_(db) {
        init_migration_table();
    }
    
    void migrate() {
        int current_version = get_current_version();
        
        // 定义迁移脚本
        std::map<int, std::string> migrations = {
            {1, R"(
                CREATE TABLE sessions (...);
                CREATE TABLE chat_messages (...);
            )"},
            {2, R"(
                CREATE TABLE vector_memory (...);
            )"},
            {3, R"(
                ALTER TABLE sessions ADD COLUMN metadata TEXT;
            )"}
        };
        
        // 执行未应用的迁移
        for (const auto& [version, sql] : migrations) {
            if (version > current_version) {
                db_.execute(sql);
                set_version(version);
            }
        }
    }

private:
    void init_migration_table() {
        db_.execute(R"(
            CREATE TABLE IF NOT EXISTS schema_migrations (
                version INTEGER PRIMARY KEY
            );
        )");
    }
    
    int get_current_version() {
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(
            db_.get_handle(),
            "SELECT MAX(version) FROM schema_migrations;",
            -1, &stmt, nullptr
        );
        
        int version = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            version = sqlite3_column_int(stmt, 0);
        }
        
        sqlite3_finalize(stmt);
        return version;
    }
    
    void set_version(int version) {
        db_.execute_prepared(
            "INSERT INTO schema_migrations (version) VALUES (?);",
            version
        );
    }
    
    Database& db_;
};
```

---

## 五、本章总结

- ✅ SQLite 数据库集成
- ✅ 会话和聊天记录持久化
- ✅ 向量存储持久化
- ✅ 数据库迁移管理
- ✅ 代码从 800 行扩展到 900 行

---

## 六、课后思考

应用已经功能完整，但缺少运维能力：
- 无法知道服务运行状态
- 不知道 API 调用量和延迟
- 出现问题无法及时发现

<details>
<summary>点击查看下一章 💡</summary>

**Step 12: 监控与可观测性**

我们将学习：
- 健康检查接口
- 指标收集（Prometheus）
- 日志系统
- 分布式追踪

</details>
