// ============================================================================
// database.hpp - PostgreSQL 数据库连接池
// ============================================================================
// 功能：提供数据库连接池、查询执行、事务管理
// 依赖：libpqxx (PostgreSQL C++ 客户端)
//
// 环境变量：
//   DATABASE_URL=postgresql://user:pass@host:port/dbname
//
// 示例：
//   Database db(Database::Config::from_env());
//   auto result = db.query("SELECT * FROM users WHERE id = $1", user_id);
// ============================================================================

#pragma once

#include <pqxx/pqxx>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <stdexcept>
#include <cstdlib>
#include <sstream>

namespace nuclaw {
namespace db {

// 数据库异常
class DatabaseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// 连接配置
struct Config {
    std::string host = "localhost";
    int port = 5432;
    std::string database;
    std::string user;
    std::string password;
    int pool_size = 10;
    
    // 从环境变量加载
    static Config from_env() {
        Config cfg;
        
        const char* url = std::getenv("DATABASE_URL");
        if (url) {
            cfg = parse_url(url);
        } else {
            // 单独的环境变量
            if (const char* h = std::getenv("DB_HOST")) cfg.host = h;
            if (const char* p = std::getenv("DB_PORT")) cfg.port = std::stoi(p);
            if (const char* d = std::getenv("DB_NAME")) cfg.database = d;
            if (const char* u = std::getenv("DB_USER")) cfg.user = u;
            if (const char* pw = std::getenv("DB_PASSWORD")) cfg.password = pw;
        }
        
        if (const char* ps = std::getenv("DB_POOL_SIZE")) {
            cfg.pool_size = std::stoi(ps);
        }
        
        return cfg;
    }
    
    // 构建连接字符串
    std::string connection_string() const {
        std::ostringstream oss;
        oss << "host=" << host 
            << " port=" << port 
            << " dbname=" << database
            << " user=" << user;
        if (!password.empty()) {
            oss << " password=" << password;
        }
        return oss.str();
    }

private:
    static Config parse_url(const std::string& url) {
        Config cfg;
        // postgresql://user:pass@host:port/dbname
        size_t proto_end = url.find("://");
        if (proto_end == std::string::npos) return cfg;
        
        size_t creds_end = url.find('@', proto_end + 3);
        size_t host_start = (creds_end == std::string::npos) ? proto_end + 3 : creds_end + 1;
        
        // 解析用户名密码
        if (creds_end != std::string::npos) {
            std::string creds = url.substr(proto_end + 3, creds_end - proto_end - 3);
            size_t colon = creds.find(':');
            if (colon != std::string::npos) {
                cfg.user = creds.substr(0, colon);
                cfg.password = creds.substr(colon + 1);
            } else {
                cfg.user = creds;
            }
        }
        
        // 解析主机端口和数据库名
        std::string host_part = url.substr(host_start);
        size_t slash = host_part.find('/');
        if (slash != std::string::npos) {
            cfg.database = host_part.substr(slash + 1);
            host_part = host_part.substr(0, slash);
        }
        
        size_t port_colon = host_part.find(':');
        if (port_colon != std::string::npos) {
            cfg.host = host_part.substr(0, port_colon);
            cfg.port = std::stoi(host_part.substr(port_colon + 1));
        } else {
            cfg.host = host_part;
        }
        
        return cfg;
    }
};

// 数据库连接池
class ConnectionPool {
public:
    explicit ConnectionPool(const Config& config) 
        : config_(config), max_size_(config.pool_size) {}
    
    ~ConnectionPool() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!pool_.empty()) {
            pool_.pop();
        }
    }
    
    // 获取连接
    std::unique_ptr<pqxx::connection> acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!pool_.empty()) {
            auto conn = std::move(pool_.front());
            pool_.pop();
            return conn;
        }
        
        // 创建新连接
        return std::make_unique<pqxx::connection>(config_.connection_string());
    }
    
    // 归还连接
    void release(std::unique_ptr<pqxx::connection> conn) {
        if (!conn || conn->is_open()) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (pool_.size() < static_cast<size_t>(max_size_)) {
                pool_.push(std::move(conn));
            }
        }
    }

private:
    Config config_;
    int max_size_;
    std::mutex mutex_;
    std::queue<std::unique_ptr<pqxx::connection>> pool_;
};

// 连接守卫（RAII）
class ConnectionGuard {
public:
    ConnectionGuard(ConnectionPool& pool) 
        : pool_(pool), conn_(pool.acquire()) {}
    
    ~ConnectionGuard() {
        if (conn_) {
            pool_.release(std::move(conn_));
        }
    }
    
    pqxx::connection& operator*() { return *conn_; }
    pqxx::connection* operator->() { return conn_.get(); }
    pqxx::connection* get() { return conn_.get(); }

private:
    ConnectionPool& pool_;
    std::unique_ptr<pqxx::connection> conn_;
};

// 数据库操作类
class Database {
public:
    explicit Database(const Config& config) 
        : pool_(std::make_unique<ConnectionPool>(config)) {}
    
    // 从环境变量创建
    static Database from_env() {
        return Database(Config::from_env());
    }
    
    // 执行查询（返回结果）
    template<typename... Args>
    pqxx::result query(const std::string& sql, Args... args) {
        try {
            ConnectionGuard conn(*pool_);
            pqxx::work txn(*conn);
            pqxx::result r = txn.exec_params(sql, args...);
            txn.commit();
            return r;
        } catch (const std::exception& e) {
            throw DatabaseError(std::string("Query failed: ") + e.what());
        }
    }
    
    // 执行更新（返回影响行数）
    template<typename... Args>
    size_t execute(const std::string& sql, Args... args) {
        try {
            ConnectionGuard conn(*pool_);
            pqxx::work txn(*conn);
            pqxx::result r = txn.exec_params(sql, args...);
            txn.commit();
            return r.affected_rows();
        } catch (const std::exception& e) {
            throw DatabaseError(std::string("Execute failed: ") + e.what());
        }
    }
    
    // 执行事务
    template<typename Func>
    void transaction(Func&& func) {
        try {
            ConnectionGuard conn(*pool_);
            pqxx::work txn(*conn);
            func(txn);
            txn.commit();
        } catch (const std::exception& e) {
            throw DatabaseError(std::string("Transaction failed: ") + e.what());
        }
    }
    
    // 测试连接
    bool ping() {
        try {
            ConnectionGuard conn(*pool_);
            pqxx::nontransaction ntx(*conn);
            pqxx::result r = ntx.exec("SELECT 1");
            return !r.empty();
        } catch (...) {
            return false;
        }
    }
    
    // 初始化表结构（示例）
    void init_schema() {
        const char* schema = R"(
            CREATE TABLE IF NOT EXISTS sessions (
                id SERIAL PRIMARY KEY,
                session_id VARCHAR(64) UNIQUE NOT NULL,
                user_id VARCHAR(64) NOT NULL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            );
            
            CREATE TABLE IF NOT EXISTS messages (
                id SERIAL PRIMARY KEY,
                session_id VARCHAR(64) REFERENCES sessions(session_id),
                role VARCHAR(16) NOT NULL,
                content TEXT NOT NULL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            );
            
            CREATE INDEX IF NOT EXISTS idx_messages_session 
                ON messages(session_id, created_at);
        )";
        
        try {
            ConnectionGuard conn(*pool_);
            pqxx::work txn(*conn);
            txn.exec(schema);
            txn.commit();
        } catch (const std::exception& e) {
            throw DatabaseError(std::string("Init schema failed: ") + e.what());
        }
    }

private:
    std::unique_ptr<ConnectionPool> pool_;
};

} // namespace db
} // namespace nuclaw
