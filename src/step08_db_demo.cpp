// ============================================================================
// step08_db_demo.cpp - Step 8 数据库集成演示
// ============================================================================
// 演进：从内存存储升级到 PostgreSQL 持久化
// 功能：会话管理、消息记录、用户信息存储
// ============================================================================

#include "nuclaw/common/http_server.hpp"
#include "nuclaw/common/llm_http_client.hpp"
#include "nuclaw/common/database.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <random>

namespace json = boost::json;
using namespace nuclaw;

// 数据访问对象
class ChatDAO {
public:
    ChatDAO(db::Database& db) : db_(db) {}
    
    // 创建会话
    std::string create_session(const std::string& user_id) {
        std::string session_id = generate_id();
        db_.execute(
            "INSERT INTO sessions (session_id, user_id) VALUES ($1, $2)",
            session_id, user_id
        );
        return session_id;
    }
    
    // 获取会话
    std::optional<json::object> get_session(const std::string& session_id) {
        auto result = db_.query(
            "SELECT session_id, user_id, created_at, updated_at FROM sessions WHERE session_id = $1",
            session_id
        );
        
        if (result.empty()) return std::nullopt;
        
        const auto& row = result[0];
        json::object session;
        session["session_id"] = row["session_id"].c_str();
        session["user_id"] = row["user_id"].c_str();
        session["created_at"] = row["created_at"].c_str();
        session["updated_at"] = row["updated_at"].c_str();
        return session;
    }
    
    // 保存消息
    void save_message(const std::string& session_id, 
                      const std::string& role,
                      const std::string& content) {
        db_.execute(
            "INSERT INTO messages (session_id, role, content) VALUES ($1, $2, $3)",
            session_id, role, content
        );
        
        // 更新会话时间
        db_.execute(
            "UPDATE sessions SET updated_at = NOW() WHERE session_id = $1",
            session_id
        );
    }
    
    // 获取会话历史
    json::array get_history(const std::string& session_id, int limit = 10) {
        json::array history;
        
        auto result = db_.query(
            "SELECT role, content, created_at FROM messages "
            "WHERE session_id = $1 ORDER BY created_at DESC LIMIT $2",
            session_id, limit
        );
        
        for (const auto& row : result) {
            json::object msg;
            msg["role"] = row["role"].c_str();
            msg["content"] = row["content"].c_str();
            msg["created_at"] = row["created_at"].c_str();
            history.push_back(std::move(msg));
        }
        
        return history;
    }
    
    // 获取用户统计
    json::object get_user_stats(const std::string& user_id) {
        auto result = db_.query(
            "SELECT COUNT(DISTINCT s.session_id) as session_count, "
            "       COUNT(m.id) as message_count "
            "FROM sessions s "
            "LEFT JOIN messages m ON s.session_id = m.session_id "
            "WHERE s.user_id = $1",
            user_id
        );
        
        json::object stats;
        if (!result.empty()) {
            stats["user_id"] = user_id;
            stats["session_count"] = result[0]["session_count"].as<int>();
            stats["message_count"] = result[0]["message_count"].as<int>();
        }
        return stats;
    }
    
    // 列出所有会话
    json::array list_sessions(int limit = 20) {
        json::array sessions;
        
        auto result = db_.query(
            "SELECT session_id, user_id, created_at, updated_at FROM sessions "
            "ORDER BY updated_at DESC LIMIT $1",
            limit
        );
        
        for (const auto& row : result) {
            json::object session;
            session["session_id"] = row["session_id"].c_str();
            session["user_id"] = row["user_id"].c_str();
            session["created_at"] = row["created_at"].c_str();
            session["updated_at"] = row["updated_at"].c_str();
            sessions.push_back(std::move(session));
        }
        
        return sessions;
    }

private:
    db::Database& db_;
    
    std::string generate_id() {
        static const char chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
        
        std::string id = "sess_";
        for (int i = 0; i < 16; ++i) {
            id += chars[dis(gen)];
        }
        return id;
    }
};

// 聊天处理器
class ChatHandler {
public:
    ChatHandler() 
        : db_(db::Database::from_env())
        , dao_(db_)
        , llm_(std::make_unique<LLMHttpClient>()) {}
    
    json::value handle_chat(const json::value& req) {
        try {
            std::string message = std::string(req.at("message").as_string());
            std::string user_id = "anonymous";
            std::string session_id;
            
            if (req.as_object().contains("user_id")) {
                user_id = std::string(req.at("user_id").as_string());
            }
            
            // 获取或创建会话
            if (req.as_object().contains("session_id")) {
                session_id = std::string(req.at("session_id").as_string());
                auto session = dao_.get_session(session_id);
                if (!session) {
                    return error_response("Session not found: " + session_id);
                }
            } else {
                session_id = dao_.create_session(user_id);
                std::cout << "[+] Created session: " << session_id << std::endl;
            }
            
            std::cout << "[" << session_id << "] " << user_id << ": " << message << std::endl;
            
            // 保存用户消息
            dao_.save_message(session_id, "user", message);
            
            // 调用 LLM（如果有配置）
            std::string reply;
            if (llm_->is_configured()) {
                std::cout << "  [🤖 Calling LLM]..." << std::flush;
                
                // 获取历史作为上下文
                auto history = dao_.get_history(session_id, 5);
                std::vector<std::pair<std::string, std::string>> hist;
                for (const auto& h : history) {
                    hist.insert(hist.begin(), {
                        std::string(h.at("role").as_string()),
                        std::string(h.at("content").as_string())
                    });
                }
                
                auto response = llm_->chat_with_history(hist, message);
                
                if (response.success) {
                    reply = response.content;
                    std::cout << " Done (" << response.latency_ms << "ms, " 
                              << response.tokens_used << " tokens)" << std::endl;
                } else {
                    reply = "Sorry, LLM error: " + response.error;
                    std::cout << " Error: " << response.error << std::endl;
                }
            } else {
                reply = "Echo: " + message + "\n\n(Configure OPENAI_API_KEY for real LLM)";
            }
            
            // 保存助手回复
            dao_.save_message(session_id, "assistant", reply);
            
            // 构建响应
            json::object resp;
            resp["reply"] = reply;
            resp["session_id"] = session_id;
            resp["user_id"] = user_id;
            resp["success"] = true;
            resp["timestamp"] = get_timestamp();
            
            return resp;
            
        } catch (const std::exception& e) {
            return error_response(e.what());
        }
    }
    
    json::value handle_get_history(const json::value& req) {
        try {
            std::string session_id = std::string(req.at("session_id").as_string());
            int limit = 10;
            if (req.as_object().contains("limit")) {
                limit = static_cast<int>(req.at("limit").as_int64());
            }
            
            auto history = dao_.get_history(session_id, limit);
            
            json::object resp;
            resp["session_id"] = session_id;
            resp["messages"] = std::move(history);
            resp["success"] = true;
            
            return resp;
            
        } catch (const std::exception& e) {
            return error_response(e.what());
        }
    }
    
    json::value handle_list_sessions(const json::value&) {
        auto sessions = dao_.list_sessions(20);
        
        json::object resp;
        resp["sessions"] = std::move(sessions);
        resp["count"] = static_cast<int>(resp["sessions"].as_array().size());
        resp["success"] = true;
        
        return resp;
    }
    
    json::value handle_get_stats(const json::value& req) {
        try {
            std::string user_id = std::string(req.at("user_id").as_string());
            auto stats = dao_.get_user_stats(user_id);
            stats["success"] = true;
            return stats;
        } catch (const std::exception& e) {
            return error_response(e.what());
        }
    }

private:
    db::Database db_;
    ChatDAO dao_;
    std::unique_ptr<LLMHttpClient> llm_;
    
    json::value error_response(const std::string& msg) {
        json::object resp;
        resp["error"] = msg;
        resp["success"] = false;
        return resp;
    }
    
    std::string get_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
};

int main() {
    try {
        std::cout << "========================================\n";
        std::cout << "  NuClaw Step 8 - Database Integration\n";
        std::cout << "========================================\n\n";
        
        // 初始化数据库
        std::cout << "[🔌 Connecting to database]..." << std::endl;
        db::Database db = db::Database::from_env();
        
        if (!db.ping()) {
            std::cerr << "❌ Database connection failed!\n";
            std::cerr << "   Set DATABASE_URL environment variable.\n";
            return 1;
        }
        std::cout << "✅ Database connected!\n\n";
        
        std::cout << "[📊 Initializing schema]..." << std::endl;
        db.init_schema();
        std::cout << "✅ Schema ready!\n\n";
        
        // 启动 HTTP 服务
        boost::asio::io_context io;
        http::Server server(io, 8080);
        ChatHandler handler;
        
        // 注册路由
        server.post("/chat", [&handler](const json::value& req) {
            return handler.handle_chat(req);
        });
        
        server.post("/history", [&handler](const json::value& req) {
            return handler.handle_get_history(req);
        });
        
        server.get("/sessions", [&handler](const json::value& req) {
            return handler.handle_list_sessions(req);
        });
        
        server.post("/stats", [&handler](const json::value& req) {
            return handler.handle_get_stats(req);
        });
        
        server.get("/health", [](const json::value&) {
            json::object resp;
            resp["status"] = "ok";
            resp["service"] = "nuclaw-step8-db";
            resp["features"] = json::array({"database", "persistence", "session_management"});
            return json::value(resp);
        });
        
        server.start();
        
        std::cout << "✅ REST API Server started on port 8080!\n\n";
        
        std::cout << "Endpoints:\n";
        std::cout << "  POST /chat          - Chat (with persistence)\n";
        std::cout << "  POST /history       - Get session history\n";
        std::cout << "  GET  /sessions      - List all sessions\n";
        std::cout << "  POST /stats         - Get user stats\n";
        std::cout << "  GET  /health        - Health check\n\n";
        
        std::cout << "Test:\n";
        std::cout << "  curl -X POST http://localhost:8080/chat \\\n";
        std::cout << "    -d '{\"message\":\"Hello\",\"user_id\":\"user001\"}'\n\n";
        
        io.run();
        
    } catch (const std::exception& e) {
        std::cerr << "[!] Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
