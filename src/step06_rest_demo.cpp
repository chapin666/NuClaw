// ============================================================================
// step06_rest_demo.cpp - Step 6 REST API 演示（使用真实 HTTP 组件）
// ============================================================================
// 本文件演示如何使用 nuclaw/common 中的 HTTP 组件
// 将原来的 WebSocket 服务器替换为 REST API 服务器
// ============================================================================

#include "nuclaw/common/http_server.hpp"
#include "nuclaw/common/llm_http_client.hpp"
#include "../../src/step06/tool_executor.hpp"
#include "../../src/step06/llm_client.hpp"
#include <iostream>
#include <sstream>

namespace json = boost::json;
using namespace nuclaw;

// 简单的 Agent 处理器
class RestAgentHandler {
public:
    json::value handle_chat(const json::value& req) {
        try {
            // 解析请求
            std::string message;
            if (req.as_object().contains("message")) {
                message = std::string(req.at("message").as_string());
            } else {
                return error_response("Missing 'message' field");
            }
            
            std::cout << "[📨 Received] " << message << std::endl;
            
            // 处理消息（使用 Step 6 的 LLMClient）
            LLMClient llm;
            std::string reply;
            
            if (llm.needs_tool(message)) {
                auto call = llm.parse_tool_call(message);
                auto result = ToolExecutor::execute(call);
                reply = llm.generate_response(message, result, call);
            } else {
                reply = llm.direct_reply(message);
            }
            
            // 构建响应
            json::object resp;
            resp["reply"] = reply;
            resp["success"] = true;
            resp["timestamp"] = get_timestamp();
            
            return resp;
            
        } catch (const std::exception& e) {
            return error_response(e.what());
        }
    }
    
    json::value handle_llm_chat(const json::value& req) {
        // 使用真实 LLM HTTP 客户端
        LLMHttpClient llm;
        
        if (!llm.is_configured()) {
            return error_response("LLM not configured. Set OPENAI_API_KEY");
        }
        
        try {
            std::string message = std::string(req.at("message").as_string());
            auto response = llm.chat(message);
            
            json::object resp;
            if (response.success) {
                resp["reply"] = response.content;
                resp["model"] = response.model;
                resp["tokens"] = response.tokens_used;
                resp["latency_ms"] = response.latency_ms;
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

private:
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
        std::cout << "  NuClaw Step 6 - REST API Demo\n";
        std::cout << "========================================\n\n";
        
        http::Server server(io, 8080);
        RestAgentHandler handler;
        
        // 注册路由
        server.post("/chat", [&handler](const json::value& req) {
            return handler.handle_chat(req);
        });
        
        server.post("/llm/chat", [&handler](const json::value& req) {
            return handler.handle_llm_chat(req);
        });
        
        server.get("/health", [](const json::value&) {
            json::object resp;
            resp["status"] = "ok";
            resp["service"] = "nuclaw-step6-rest";
            return json::value(resp);
        });
        
        server.start();
        
        std::cout << "✅ REST API Server started!\n\n";
        std::cout << "Endpoints:\n";
        std::cout << "  POST /chat       - 使用本地 Mock LLM\n";
        std::cout << "  POST /llm/chat   - 使用真实 OpenAI API\n";
        std::cout << "  GET  /health     - 健康检查\n\n";
        
        std::cout << "Test commands:\n";
        std::cout << "  curl -X POST http://localhost:8080/chat \\\n";
        std::cout << "    -H 'Content-Type: application/json' \\\n";
        std::cout << "    -d '{\"message\":\"北京天气如何?\"}'\n\n";
        
        if (std::getenv("OPENAI_API_KEY")) {
            std::cout << "  curl -X POST http://localhost:8080/llm/chat \\\n";
            std::cout << "    -H 'Content-Type: application/json' \\\n";
            std::cout << "    -d '{\"message\":\"你好\"}'\n\n";
        } else {
            std::cout << "⚠️  Set OPENAI_API_KEY to use /llm/chat\n\n";
        }
        
        io.run();
        
    } catch (const std::exception& e) {
        std::cerr << "[!] Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
