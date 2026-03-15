// ============================================================================
// Step 8: 工具安全与沙箱 + 数据库持久化 - 基于 Step 7 演进
// ============================================================================
//
// 演进说明：
//   Step 7: 异步执行 + 并发控制
//   Step 8: + 安全沙箱 + 新工具 + 数据库持久化
//
// 新增：
//   - sandbox.hpp:     安全沙箱（SSRF/路径遍历/代码注入防护）
//   - http_tool.hpp:   HTTP 工具（带 SSRF 防护）
//   - file_tool.hpp:   文件工具（带路径遍历防护）
//   - code_tool.hpp:   代码执行（带黑名单防护）
//   - 数据库集成:      对话历史持久化
//
// 编译: mkdir build && cd build && cmake .. && make
// 运行: ./nuclaw_step08
// 测试: curl -X POST http://localhost:8080/chat -d '{"message":"北京天气"}'
// ============================================================================

#include "chat_engine.hpp"
#include "tool_executor.hpp"
#include "llm_client.hpp"
#include "sandbox.hpp"
#include "common/http_server.hpp"
#include "common/llm_http_client.hpp"
#include "common/database.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <thread>
#include <memory>

namespace json = boost::json;
using namespace nuclaw;
using nuclaw::db::Database;  // 添加 Database 命名空间

// Agent 处理器（支持安全沙箱和数据库）
class SecureAgentHandler {
public:
    SecureAgentHandler() : chat_engine_(std::make_unique<ChatEngine>()) {
        // 尝试连接数据库
        try {
            db_ = std::make_unique<Database>(Database::from_env());
            if (db_->ping()) {
                db_available_ = true;
                db_->init_schema();
                std::cout << "[✅ Database] Connected" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "[⚠️  Database] Not available: " << e.what() << std::endl;
            std::cout << "               Set DATABASE_URL to enable persistence" << std::endl;
        }
    }
    
    // 使用本地 Mock LLM 处理
    json::value handle_chat_mock(const json::value& req) {
        try {
            std::string message = get_message(req);
            std::string session_id = get_session_id(req, "default");
            
            std::cout << "[📨 Mock Secure] " << message << std::endl;
            
            // 安全检查
            SecurityCheck check = Sandbox::check_input(message);
            if (!check.safe) {
                log_message(session_id, message, "BLOCKED: " + check.reason);
                return error_response("安全风险: " + check.reason);
            }
            
            // 处理消息
            LLMClient llm;
            std::string reply;
            
            if (llm.needs_tool(message)) {
                auto call = llm.parse_tool_call(message);
                // 使用安全执行
                auto result = Sandbox::execute_secure(call);
                reply = llm.generate_response(message, result, call);
            } else {
                reply = llm.direct_reply(message);
            }
            
            // 持久化
            log_message(session_id, message, reply);
            
            return success_response(reply, "mock");
            
        } catch (const std::exception& e) {
            return error_response(e.what());
        }
    }
    
    // 使用真实 HTTP LLM 处理
    json::value handle_chat_real(const json::value& req) {
        LLMHttpClient llm;
        
        if (!llm.is_configured()) {
            return error_response(
                "LLM not configured. Set OPENAI_API_KEY environment variable.\n"
                "Or use POST /chat for Mock LLM."
            );
        }
        
        try {
            std::string message = get_message(req);
            std::string session_id = get_session_id(req, "default");
            
            // 安全检查
            SecurityCheck check = Sandbox::check_input(message);
            if (!check.safe) {
                log_message(session_id, message, "BLOCKED: " + check.reason);
                return error_response("安全风险: " + check.reason);
            }
            
            std::cout << "[📨 Real LLM Secure] " << message << std::endl;
            
            auto response = llm.chat(message);
            
            if (response.success) {
                // 持久化
                log_message(session_id, message, response.content);
                
                json::object resp;
                resp["reply"] = response.content;
                resp["model"] = response.model;
                resp["tokens_used"] = response.tokens_used;
                resp["latency_ms"] = response.latency_ms;
                resp["provider"] = llm.config().provider;
                resp["success"] = true;
                resp["timestamp"] = get_timestamp();
                return resp;
            } else {
                return error_response(response.error);
            }
            
        } catch (const std::exception& e) {
            return error_response(e.what());
        }
    }
    
    // 批量异步处理
    json::value handle_batch_async(const json::value& req) {
        try {
            auto& messages = req.as_object().at("messages").as_array();
            if (messages.empty()) {
                return error_response("Missing 'messages' array");
            }
            
            std::cout << "[📨 Batch Secure] " << messages.size() << " requests" << std::endl;
            
            std::atomic<int> completed{0};
            int total = messages.size();
            std::vector<std::string> results(total);
            std::atomic<int> blocked{0};
            
            auto start = std::chrono::steady_clock::now();
            
            // 提交异步任务
            for (size_t i = 0; i < messages.size(); i++) {
                std::string msg = std::string(messages[i].as_string());
                
                // 安全检查
                SecurityCheck check = Sandbox::check_input(msg);
                if (!check.safe) {
                    results[i] = "[BLOCKED: " + check.reason + "]";
                    completed++;
                    blocked++;
                    continue;
                }
                
                chat_engine_->process_async(msg, ctx_, 
                    [&completed, &results, i](std::string reply) {
                        results[i] = reply;
                        completed++;
                    });
            }
            
            // 等待完成
            while (completed < total) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            
            auto end = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            
            json::object resp;
            resp["success"] = true;
            resp["batch_size"] = total;
            resp["blocked"] = blocked.load();
            resp["elapsed_ms"] = elapsed;
            json::array replies;
            for (const auto& r : results) {
                replies.push_back(json::value(r));
            }
            resp["replies"] = replies;
            resp["timestamp"] = get_timestamp();
            return resp;
            
        } catch (const std::exception& e) {
            return error_response(e.what());
        }
    }
    
    // 获取对话历史
    json::value get_history(const json::value& req) {
        if (!db_available_) {
            return error_response("Database not configured");
        }
        
        try {
            std::string session_id = "default";
            if (req.as_object().contains("session_id")) {
                session_id = std::string(req.at("session_id").as_string());
            }
            
            auto result = db_->query(
                "SELECT user_message, ai_reply, created_at FROM chat_history "
                "WHERE session_id = $1 ORDER BY created_at DESC LIMIT 20",
                session_id
            );
            
            json::array history;
            for (const auto& row : result) {
                json::object msg;
                msg["user"] = row["user_message"].c_str();
                msg["ai"] = row["ai_reply"].c_str();
                msg["time"] = row["created_at"].c_str();
                history.push_back(msg);
            }
            
            json::object resp;
            resp["success"] = true;
            resp["session_id"] = session_id;
            resp["history"] = history;
            return resp;
            
        } catch (const std::exception& e) {
            return error_response(e.what());
        }
    }
    
    // 获取当前配置状态
    json::value get_config() const {
        LLMHttpClient llm;
        
        json::object resp;
        resp["step"] = 8;
        resp["mock_llm"] = true;
        resp["real_llm_configured"] = llm.is_configured();
        resp["database_available"] = db_available_;
        
        if (llm.is_configured()) {
            resp["provider"] = llm.config().provider;
            resp["model"] = llm.config().model;
        }
        
        resp["endpoints"] = json::array({
            json::object({
                {"path", "/chat"},
                {"method", "POST"},
                {"description", "使用本地 Mock LLM（无需配置）"},
                {"available", true}
            }),
            json::object({
                {"path", "/llm/chat"},
                {"method", "POST"},
                {"description", "使用真实 HTTP LLM（需 OPENAI_API_KEY）"},
                {"available", llm.is_configured()}
            }),
            json::object({
                {"path", "/batch"},
                {"method", "POST"},
                {"description", "批量异步处理"},
                {"available", true}
            }),
            json::object({
                {"path", "/history"},
                {"method", "POST"},
                {"description", "查询对话历史（需 DATABASE_URL）"},
                {"available", db_available_}
            })
        });
        
        return resp;
    }

private:
    std::unique_ptr<ChatEngine> chat_engine_;
    ChatContext ctx_;
    std::unique_ptr<Database> db_;
    bool db_available_ = false;
    
    std::string get_message(const json::value& req) {
        if (!req.as_object().contains("message")) {
            throw std::runtime_error("Missing 'message' field");
        }
        return std::string(req.at("message").as_string());
    }
    
    std::string get_session_id(const json::value& req, const std::string& default_id) {
        if (req.as_object().contains("session_id")) {
            return std::string(req.at("session_id").as_string());
        }
        return default_id;
    }
    
    void log_message(const std::string& session_id, 
                     const std::string& user_msg, 
                     const std::string& ai_reply) {
        if (!db_available_) return;
        
        try {
            db_->execute(
                "INSERT INTO chat_history (session_id, user_message, ai_reply) VALUES ($1, $2, $3)",
                session_id, user_msg, ai_reply
            );
        } catch (...) {
            // 静默失败，不阻断主流程
        }
    }
    
    json::value success_response(const std::string& reply, const std::string& type) {
        json::object resp;
        resp["reply"] = reply;
        resp["type"] = type;
        resp["success"] = true;
        resp["timestamp"] = get_timestamp();
        return resp;
    }
    
    json::value error_response(const std::string& msg) {
        json::object resp;
        resp["error"] = msg;
        resp["success"] = false;
        return resp;
    }
    
    std::string get_timestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
};

int main() {
    try {
        boost::asio::io_context io;
        
        std::cout << "========================================" << std::endl;
        std::cout << "  NuClaw Step 8 - Tool Security + DB" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << std::endl;
        
        nuclaw::http::Server server(io, 8080);
        SecureAgentHandler handler;
        
        // 注册路由
        server.post("/chat", [&handler](const json::value& req) {
            return handler.handle_chat_mock(req);
        });
        
        server.post("/llm/chat", [&handler](const json::value& req) {
            return handler.handle_chat_real(req);
        });
        
        server.post("/batch", [&handler](const json::value& req) {
            return handler.handle_batch_async(req);
        });
        
        server.post("/history", [&handler](const json::value& req) {
            return handler.get_history(req);
        });
        
        server.get("/config", [&handler](const json::value&) {
            return handler.get_config();
        });
        
        server.get("/health", [](const json::value&) {
            json::object resp;
            resp["status"] = "ok";
            resp["step"] = 8;
            resp["features"] = json::array({
                "security_sandbox", "async_execution", 
                "batch_processing", "database_persistence",
                "mock_llm", "real_llm"
            });
            return json::value(resp);
        });
        
        server.start();
        
        // 打印状态
        auto config = handler.get_config();
        bool real_llm_ready = config.at("real_llm_configured").as_bool();
        bool db_ready = config.at("database_available").as_bool();
        
        std::cout << std::endl;
        std::cout << "✅ REST API Server started on port 8080!" << std::endl;
        std::cout << std::endl;
        
        std::cout << "🛡️  演进对比：" << std::endl;
        std::cout << "  Step 7: 异步执行 + 并发控制" << std::endl;
        std::cout << "  Step 8: + 安全沙箱 + 数据库持久化" << std::endl;
        std::cout << std::endl;
        
        std::cout << Sandbox::get_sandbox_info() << std::endl;
        std::cout << std::endl;
        
        std::cout << "📡 Endpoints:" << std::endl;
        std::cout << "  POST /chat       - Mock LLM (无需配置) ✅" << std::endl;
        std::cout << "  POST /llm/chat   - Real LLM" 
                  << (real_llm_ready ? " ✅" : " ⚠️  (需 OPENAI_API_KEY)") << std::endl;
        std::cout << "  POST /batch      - 批量异步处理 ✅" << std::endl;
        std::cout << "  POST /history    - 对话历史" 
                  << (db_ready ? " ✅" : " ⚠️  (需 DATABASE_URL)") << std::endl;
        std::cout << "  GET  /config     - 查看配置" << std::endl;
        std::cout << "  GET  /health     - 健康检查" << std::endl;
        std::cout << std::endl;
        
        std::cout << "🧪 Test:" << std::endl;
        std::cout << "  curl -X POST http://localhost:8080/chat \\" << std::endl;
        std::cout << "    -H 'Content-Type: application/json' \\" << std::endl;
        std::cout << "    -d '{\"message\":\"北京天气如何\"}'" << std::endl;
        std::cout << std::endl;
        
        std::cout << "🛡️  安全测试（会被拦截）:" << std::endl;
        std::cout << "  curl -X POST http://localhost:8080/chat \\" << std::endl;
        std::cout << "    -d '{\"message\":\"http_get http://localhost/admin\"}'" << std::endl;
        std::cout << std::endl;
        
        std::cout << "📚 演进：" << std::endl;
        std::cout << "  Step 7 → 8: 异步 + 安全 + 持久化" << std::endl;
        std::cout << "  下一章：Agent 状态管理" << std::endl;
        std::cout << std::endl;
        
        io.run();
        
    } catch (const std::exception& e) {
        std::cerr << "[!] Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
