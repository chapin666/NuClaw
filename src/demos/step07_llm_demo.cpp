// ============================================================================
// step07_llm_demo.cpp - Step 7 真实 LLM HTTP 调用演示
// ============================================================================
// 演进：从 Mock LLM 升级到真实 HTTP LLM 调用
// 使用 nuclaw::LLMHttpClient 调用 OpenAI/Moonshot API
// ============================================================================

#include "nuclaw/common/http_server.hpp"
#include "nuclaw/common/llm_http_client.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace json = boost::json;
using namespace nuclaw;

// 带历史记录的对话管理
class ConversationManager {
public:
    struct Session {
        std::vector<std::pair<std::string, std::string>> history;
        std::chrono::steady_clock::time_point last_active;
    };
    
    std::map<std::string, Session> sessions;
    std::mutex mutex;
    
    Session& get_or_create(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex);
        auto& session = sessions[session_id];
        session.last_active = std::chrono::steady_clock::now();
        return session;
    }
};

class LLMHandler {
public:
    LLMHandler() : llm_(std::make_unique<LLMHttpClient>()) {}
    
    json::value handle_chat(const json::value& req) {
        // 检查 LLM 配置
        if (!llm_->is_configured()) {
            return error_response(
                "LLM not configured. Set OPENAI_API_KEY or MOONSHOT_API_KEY environment variable."
            );
        }
        
        try {
            // 解析请求
            std::string message = std::string(req.at("message").as_string());
            std::string session_id = "default";
            
            if (req.as_object().contains("session_id")) {
                session_id = std::string(req.at("session_id").as_string());
            }
            
            std::cout << "[📨 " << session_id << "] " << message << std::endl;
            
            // 获取或创建会话
            auto& session = conversations_.get_or_create(session_id);
            
            // 调用真实 LLM API
            std::cout << "  [🤖 Calling " << llm_->config().provider << " API]..." << std::flush;
            
            auto response = llm_->chat_with_history(session.history, message);
            
            std::cout << " Done (" << response.latency_ms << "ms)" << std::endl;
            
            // 保存历史
            session.history.push_back({"user", message});
            session.history.push_back({"assistant", response.content});
            
            // 限制历史长度（保留最近 10 轮）
            if (session.history.size() > 20) {
                session.history.erase(session.history.begin(), session.history.begin() + 2);
            }
            
            // 构建响应
            json::object resp;
            if (response.success) {
                resp["reply"] = response.content;
                resp["model"] = response.model;
                resp["tokens_used"] = response.tokens_used;
                resp["latency_ms"] = response.latency_ms;
                resp["session_id"] = session_id;
                resp["history_count"] = static_cast<int>(session.history.size() / 2);
                resp["success"] = true;
            } else {
                resp["error"] = response.error;
                resp["success"] = false;
            }
            resp["timestamp"] = get_timestamp();
            
            return resp;
            
        } catch (const std::exception& e) {
            return error_response(e.what());
        }
    }
    
    json::value get_config() const {
        json::object resp;
        resp["provider"] = llm_->config().provider;
        resp["model"] = llm_->config().model;
        resp["configured"] = llm_->is_configured();
        resp["base_url"] = llm_->config().base_url;
        return resp;
    }

private:
    std::unique_ptr<LLMHttpClient> llm_;
    ConversationManager conversations_;
    
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
        boost::asio::io_context io;
        
        std::cout << "========================================\n";
        std::cout << "  NuClaw Step 7 - Real LLM HTTP Demo\n";
        std::cout << "========================================\n\n";
        
        http::Server server(io, 8080);
        LLMHandler handler;
        
        // 注册路由
        server.post("/chat", [&handler](const json::value& req) {
            return handler.handle_chat(req);
        });
        
        server.get("/config", [&handler](const json::value&) {
            return handler.get_config();
        });
        
        server.get("/health", [](const json::value&) {
            json::object resp;
            resp["status"] = "ok";
            resp["service"] = "nuclaw-step7-llm";
            resp["features"] = json::array({"real_llm", "conversation_history"});
            return json::value(resp);
        });
        
        server.start();
        
        // 打印配置信息
        auto config = handler.get_config();
        std::cout << "📊 LLM Configuration:\n";
        std::cout << "  Provider: " << config.at("provider").as_string() << "\n";
        std::cout << "  Model: " << config.at("model").as_string() << "\n";
        std::cout << "  Configured: " << (config.at("configured").as_bool() ? "✅" : "❌") << "\n\n";
        
        std::cout << "✅ REST API Server started on port 8080!\n\n";
        
        std::cout << "Endpoints:\n";
        std::cout << "  POST /chat       - Chat with real LLM\n";
        std::cout << "  GET  /config     - LLM configuration\n";
        std::cout << "  GET  /health     - Health check\n\n";
        
        std::cout << "Test:\n";
        std::cout << "  curl -X POST http://localhost:8080/chat \\\n";
        std::cout << "    -H 'Content-Type: application/json' \\\n";
        std::cout << "    -d '{\"message\":\"你好，请介绍你自己\",\"session_id\":\"test001\"}'\n\n";
        
        if (!config.at("configured").as_bool()) {
            std::cout << "⚠️  WARNING: LLM not configured!\n";
            std::cout << "   Set OPENAI_API_KEY or MOONSHOT_API_KEY to enable LLM.\n\n";
        }
        
        io.run();
        
    } catch (const std::exception& e) {
        std::cerr << "[!] Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
