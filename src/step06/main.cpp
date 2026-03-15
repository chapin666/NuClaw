// ============================================================================
// Step 6: 工具调用 - 完整的 Agent Loop + REST API
// ============================================================================
// 
// 本章演进：
// - 从 WebSocket 升级到 REST API（更通用，易于测试）
// - 支持本地 Mock LLM（默认）和真实 HTTP LLM（配置后）
// - 工具调用系统（天气、时间、计算）
//
// 编译: mkdir build && cd build && cmake .. && make
// 运行: ./nuclaw_step06
// 测试: curl -X POST http://localhost:8080/chat -d '{"message":"北京天气"}'
// ============================================================================

#include "chat_engine.hpp"
#include "tool_executor.hpp"
#include "llm_client.hpp"
#include "common/http_server.hpp"
#include "common/llm_http_client.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace json = boost::json;
using namespace nuclaw;

// Agent 处理器
class AgentHandler {
public:
    // 使用本地 Mock LLM 处理（无需配置，默认）
    json::value handle_chat_mock(const json::value& req) {
        try {
            std::string message = get_message(req);
            std::cout << "[📨 Mock] " << message << std::endl;
            
            // 使用 Step 6 的本地 LLMClient
            LLMClient llm;
            std::string reply;
            
            if (llm.needs_tool(message)) {
                auto call = llm.parse_tool_call(message);
                auto result = ToolExecutor::execute(call);
                reply = llm.generate_response(message, result, call);
            } else {
                reply = llm.direct_reply(message);
            }
            
            return success_response(reply, "mock");
            
        } catch (const std::exception& e) {
            return error_response(e.what());
        }
    }
    
    // 使用真实 HTTP LLM 处理（需配置 OPENAI_API_KEY）
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
            std::cout << "[📨 Real LLM] " << message << std::endl;
            
            auto response = llm.chat(message);
            
            if (response.success) {
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
    
    // 获取当前配置状态
    json::value get_config() const {
        LLMHttpClient llm;
        
        json::object resp;
        resp["mock_llm"] = true;  // 总是可用
        resp["real_llm_configured"] = llm.is_configured();
        
        if (llm.is_configured()) {
            resp["provider"] = llm.config().provider;
            resp["model"] = llm.config().model;
            resp["base_url"] = llm.config().base_url;
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
            })
        });
        
        return resp;
    }

private:
    std::string get_message(const json::value& req) {
        if (!req.as_object().contains("message")) {
            throw std::runtime_error("Missing 'message' field");
        }
        return std::string(req.at("message").as_string());
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
        
        std::cout << "========================================\n";
        std::cout << "  NuClaw Step 6 - Tool Calling\n";
        std::cout << "========================================\n\n";
        
        http::Server server(io, 8080);
        AgentHandler handler;
        
        // 注册路由
        server.post("/chat", [&handler](const json::value& req) {
            return handler.handle_chat_mock(req);
        });
        
        server.post("/llm/chat", [&handler](const json::value& req) {
            return handler.handle_chat_real(req);
        });
        
        server.get("/config", [&handler](const json::value&) {
            return handler.get_config();
        });
        
        server.get("/health", [](const json::value&) {
            json::object resp;
            resp["status"] = "ok";
            resp["step"] = 6;
            resp["features"] = json::array({"tool_calling", "mock_llm", "real_llm"});
            return json::value(resp);
        });
        
        server.start();
        
        // 打印状态
        auto config = handler.get_config();
        bool real_llm_ready = config.at("real_llm_configured").as_bool();
        
        std::cout << "✅ REST API Server started on port 8080!\n\n";
        
        std::cout << "🛠️ 工具支持：\n";
        std::cout << "  - 天气查询: \"北京天气如何\"\n";
        std::cout << "  - 时间查询: \"现在几点\"\n";
        std::cout << "  - 计算工具: \"25 * 4 等于多少\"\n\n";
        
        std::cout << "📡 Endpoints:\n";
        std::cout << "  POST /chat       - Mock LLM (无需配置) ✅\n";
        std::cout << "  POST /llm/chat   - Real LLM" 
                  << (real_llm_ready ? " ✅" : " ⚠️  (需 OPENAI_API_KEY)") << "\n";
        std::cout << "  GET  /config     - 查看配置\n";
        std::cout << "  GET  /health     - 健康检查\n\n";
        
        std::cout << "🧪 Test:\n";
        std::cout << "  curl -X POST http://localhost:8080/chat \\\n";
        std::cout << "    -H 'Content-Type: application/json' \\\n";
        std::cout << "    -d '{\"message\":\"北京天气如何\"}'\n\n";
        
        if (real_llm_ready) {
            std::cout << "  curl -X POST http://localhost:8080/llm/chat \\\n";
            std::cout << "    -H 'Content-Type: application/json' \\\n";
            std::cout << "    -d '{\"message\":\"你好\"}'\n\n";
        }
        
        std::cout << "📚 演进：\n";
        std::cout << "  WebSocket → REST API (更通用)\n";
        std::cout << "  Mock LLM → 可选真实 LLM\n";
        std::cout << "  下一章：异步工具执行\n\n";
        
        io.run();
        
    } catch (const std::exception& e) {
        std::cerr << "[!] Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
