// src/step05/main.cpp
// Step 5: Multi-Tenant Architecture
// 编译: g++ -std=c++17 main.cpp -o server -lboost_system -lpqxx -lpthread

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <pqxx/pqxx>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

// 租户上下文
struct Tenant {
    std::string id;
    std::string name;
    std::string schema;
};

// 租户管理器
class TenantManager {
public:
    TenantManager(const std::string& conn_str) : conn_str_(conn_str) {
        init_db();
    }
    
    // 创建租户
    Tenant create(const std::string& name) {
        boost::uuids::uuid uuid = boost::uuids::random_generator()();
        std::string id = "t_" + boost::uuids::to_string(uuid).substr(0, 8);
        std::string schema = "tenant_" + id;
        
        pqxx::connection conn(conn_str_);
        pqxx::work txn(conn);
        
        // 创建 schema
        txn.exec("CREATE SCHEMA IF NOT EXISTS " + txn.quote_name(schema));
        
        // 记录租户
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS public.tenants (
                id VARCHAR(50) PRIMARY KEY,
                name VARCHAR(255),
                schema_name VARCHAR(255),
                created_at TIMESTAMP DEFAULT NOW()
            )
        )");
        
        txn.exec_params(
            "INSERT INTO public.tenants (id, name, schema_name) VALUES ($1, $2, $3)",
            id, name, schema
        );
        
        // 创建租户表
        create_tables(txn, schema);
        txn.commit();
        
        return Tenant{id, name, schema};
    }
    
    // 在租户上下文中执行
    template<typename F>
    auto with_tenant(const std::string& tenant_id, F&& f) -> decltype(auto) {
        pqxx::connection conn(conn_str_);
        pqxx::work txn(conn);
        
        // 获取 schema
        auto result = txn.exec_params(
            "SELECT schema_name FROM public.tenants WHERE id = $1",
            tenant_id
        );
        
        if (result.empty()) {
            throw std::runtime_error("Tenant not found");
        }
        
        std::string schema = result[0][0].as<std::string>();
        txn.exec("SET search_path TO " + txn.quote_name(schema) + ", public");
        
        return f(txn);
    }
    
private:
    void init_db() {
        pqxx::connection conn(conn_str_);
        pqxx::work txn(conn);
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS public.tenants (
                id VARCHAR(50) PRIMARY KEY,
                name VARCHAR(255),
                schema_name VARCHAR(255) UNIQUE,
                created_at TIMESTAMP DEFAULT NOW()
            )
        )");
        txn.commit();
    }
    
    void create_tables(pqxx::work& txn, const std::string& schema) {
        txn.exec("CREATE TABLE IF NOT EXISTS " + txn.quote_name(schema) + ".users ("
            "id SERIAL PRIMARY KEY,"
            "username VARCHAR(255) UNIQUE,"
            "email VARCHAR(255),"
            "created_at TIMESTAMP DEFAULT NOW()"
        ")");
        
        txn.exec("CREATE TABLE IF NOT EXISTS " + txn.quote_name(schema) + ".sessions ("
            "id VARCHAR(50) PRIMARY KEY,"
            "user_id INTEGER,"
            "title VARCHAR(255),"
            "created_at TIMESTAMP DEFAULT NOW()"
        ")");
        
        txn.exec("CREATE TABLE IF NOT EXISTS " + txn.quote_name(schema) + ".messages ("
            "id SERIAL PRIMARY KEY,"
            "session_id VARCHAR(50),"
            "role VARCHAR(50),"
            "content TEXT,"
            "created_at TIMESTAMP DEFAULT NOW()"
        ")");
    }
    
    std::string conn_str_;
};

// 多租户 Agent
class MultiAgent {
public:
    MultiAgent(TenantManager& tm) : tenant_mgr_(tm) {}
    
    std::string chat(const std::string& tenant_id, const std::string& user, 
                     const std::string& msg) {
        try {
            // 保存用户消息
            tenant_mgr_.with_tenant(tenant_id, [&](pqxx::work& txn) {
                std::string session = "s_" + user;
                
                txn.exec_params(
                    "INSERT INTO messages (session_id, role, content) VALUES ($1, 'user', $2)",
                    session, msg
                );
                txn.commit();
            });
            
            // 生成响应 (简化)
            std::string response = "[T-" + tenant_id + "] " + msg;
            
            // 保存助手消息
            tenant_mgr_.with_tenant(tenant_id, [&](pqxx::work& txn) {
                std::string session = "s_" + user;
                txn.exec_params(
                    "INSERT INTO messages (session_id, role, content) VALUES ($1, 'assistant', $2)",
                    session, response
                );
                txn.commit();
            });
            
            return response;
            
        } catch (const std::exception& e) {
            return std::string("Error: ") + e.what();
        }
    }
    
    int get_message_count(const std::string& tenant_id) {
        return tenant_mgr_.with_tenant(tenant_id, [](pqxx::work& txn) {
            auto r = txn.exec("SELECT COUNT(*) FROM messages");
            return r[0][0].as<int>();
        });
    }
    
private:
    TenantManager& tenant_mgr_;
};

int main() {
    const char* db_url = std::getenv("DATABASE_URL");
    if (!db_url) {
        std::cerr << "Set DATABASE_URL, e.g.:\n";
        std::cerr << "export DATABASE_URL=postgresql://user:pass@localhost/nuclaw\n";
        return 1;
    }
    
    try {
        TenantManager tm(db_url);
        MultiAgent agent(tm);
        
        std::cout << "NuClaw Step 5 - Multi-Tenant\n\n";
        
        // 创建租户
        auto t1 = tm.create("Acme Corp");
        auto t2 = tm.create("Globex Inc");
        
        std::cout << "Created tenants:\n";
        std::cout << "  " << t1.id << " (" << t1.name << ")\n";
        std::cout << "  " << t2.id << " (" << t2.name << ")\n\n";
        
        // 模拟对话
        std::cout << "=== Tenant A (Acme) ===\n";
        std::cout << "Alice: Hello\n";
        std::cout << "Agent: " << agent.chat(t1.id, "alice", "Hello") << "\n";
        
        std::cout << "\n=== Tenant B (Globex) ===\n";
        std::cout << "Bob: Hi\n";
        std::cout << "Agent: " << agent.chat(t2.id, "bob", "Hi") << "\n";
        
        std::cout << "\nMessage counts:\n";
        std::cout << "  Tenant A: " << agent.get_message_count(t1.id) << "\n";
        std::cout << "  Tenant B: " << agent.get_message_count(t2.id) << "\n";
        
        std::cout << "\n✅ Multi-tenant demo complete\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
