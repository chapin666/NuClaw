// ============================================================================
// test_database.cpp - 数据库组件测试
// ============================================================================

#include "nuclaw/common/database.hpp"
#include <iostream>

using namespace nuclaw;

int main() {
    std::cout << "========================================\n";
    std::cout << "数据库组件测试\n";
    std::cout << "========================================\n\n";
    
    // 测试 1：配置解析
    std::cout << "[测试 1] 配置解析\n";
    {
        db::Config cfg = db::Config::from_env();
        std::cout << "  Host: " << cfg.host << "\n";
        std::cout << "  Port: " << cfg.port << "\n";
        std::cout << "  Database: " << cfg.database << "\n";
        std::cout << "  User: " << cfg.user << "\n";
        std::cout << "  Pool Size: " << cfg.pool_size << "\n";
        std::cout << "  Connection String: " << cfg.connection_string() << "\n\n";
    }
    
    // 测试 2：连接测试
    std::cout << "[测试 2] 数据库连接\n";
    try {
        db::Database db = db::Database::from_env();
        
        if (db.ping()) {
            std::cout << "  ✅ 连接成功\n\n";
            
            // 测试 3：初始化表结构
            std::cout << "[测试 3] 初始化表结构\n";
            db.init_schema();
            std::cout << "  ✅ 表结构初始化完成\n\n";
            
            // 测试 4：插入数据
            std::cout << "[测试 4] 插入数据\n";
            size_t rows = db.execute(
                "INSERT INTO sessions (session_id, user_id) VALUES ($1, $2) "
                "ON CONFLICT (session_id) DO NOTHING",
                "test_session_001", "test_user_001"
            );
            std::cout << "  插入行数: " << rows << "\n\n";
            
            // 测试 5：查询数据
            std::cout << "[测试 5] 查询数据\n";
            auto result = db.query(
                "SELECT session_id, user_id, created_at FROM sessions WHERE session_id = $1",
                "test_session_001"
            );
            std::cout << "  查询结果行数: " << result.size() << "\n";
            for (const auto& row : result) {
                std::cout << "    Session: " << row["session_id"].c_str() << "\n";
                std::cout << "    User: " << row["user_id"].c_str() << "\n";
                std::cout << "    Created: " << row["created_at"].c_str() << "\n";
            }
            std::cout << "\n";
            
            // 测试 6：事务
            std::cout << "[测试 6] 事务处理\n";
            db.transaction([](pqxx::work& txn) {
                txn.exec_params(
                    "INSERT INTO messages (session_id, role, content) VALUES ($1, $2, $3)",
                    "test_session_001", "user", "Hello from transaction"
                );
                std::cout << "  ✅ 事务执行成功\n";
            });
            std::cout << "\n";
            
        } else {
            std::cout << "  ❌ 连接失败（请检查 DATABASE_URL 环境变量）\n\n";
        }
    } catch (const std::exception& e) {
        std::cout << "  ❌ 错误: " << e.what() << "\n\n";
    }
    
    std::cout << "========================================\n";
    std::cout << "测试完成\n";
    std::cout << "========================================\n";
    
    return 0;
}
