// ============================================================================
// Step 7: 异步工具执行 - 基于 Step 6 演进
// ============================================================================
//
// 演进说明：
//   Step 6: 同步执行 → 阻塞等待
//   Step 7: 异步执行 → 立即返回 + 回调 + 并发控制
//
// 关键变化：
//   - 从 WebSocket 升级到 REST API
//   - tool_executor.hpp: 新增 execute_async(), 并发控制, 超时
//   - chat_engine.hpp: 新增 process_async(), 支持回调
//   - 保留真实 LLM 支持（配置后可用）
//
// 编译: mkdir build && cd build && cmake .. && make
// 运行: ./nuclaw_step07
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
#include <chrono>
#include <thread>

namespace json = boost::json;
using namespace nuclaw;

// Agent 处理器（支持异步）
class AsyncAgentHandler {
public:
    AsyncAgentHandler() : chat_engine_(std::make_unique<ChatEngine>()) {}
    
    // 使用本地 Mock LLM 处理
    json::value handle_chat_mock(const json::value& req) {
        try {
            std::string message = get_message(req);
            std::cout << "[📨 Mock Async] " << message << std::endl;
            
            // 同步处理（保持简单）
            LLMClient llm;
            std::string reply;
            
            if (llm.needs_tool(message)) {
                auto call = llm.parse_tool_call(message);
                auto result = ToolExecutor::execute_sync(call);
                reply = llm.generate_response(message, result, call);
            } else {
                reply = llm.direct_reply(message);
            }
            
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
            std::cout << "[📨 Real LLM Async] " << message << std::endl;
            
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
    
    // 异步批量执行演示
    json::value handle_batch_async(const json::value& req) {
        try {
            auto& messages = req.as_object().at("messages").as_array();
            if (messages.empty()) {
                return error_response("Missing 'messages' array");
            }
            
            std::cout << "[📨 Batch Async] " << messages.size() << " requests" << std::endl;
            
            std::atomic<int> completed{0};
            int total = messages.size();
            std::vector<std::string> results(total);
            
            auto start = std::chrono::steady_clock::now();
            
            // 提交异步任务
            for (size_t i = 0; i < messages.size(); i++) {
                std::string msg = std::string(messages[i].as_string());
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
    
    // 获取当前配置状态
    json::value get_config() const {
        LLMHttpClient llm;
        
        json::object resp;
        resp["step"] = 7;
        resp["mock_llm"] = true;
        resp["real_llm_configured"] = llm.is_configured();
        
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
                {"description", "批量异步处理（演示并发能力）"},
                {"available", true}
            })
        });
        
        return resp;
    }

private:
    std::unique_ptr<ChatEngine> chat_engine_;
    ChatContext ctx_;
    
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
        
        std::cout << "========================================" << std::endl;
        std::cout << "  NuClaw Step 7 - Async Tool Execution" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << std::endl;
        
        http::Server server(io, 8080);
        AsyncAgentHandler handler;
        
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
        
        server.get("/config", [&handler](const json::value&) {
            return handler.get_config();
        });
        
        server.get("/health", [](const json::value&) {
            json::object resp;
            resp["status"] = "ok";
            resp["step"] = 7;
            resp["features"] = json::array({"async_execution", "batch_processing", "mock_llm", "real_llm"});
            return json::value(resp);
        });
        
        server.start();
        
        // 打印状态
        auto config = handler.get_config();
        bool real_llm_ready = config.at("real_llm_configured").as_bool();
        
        std::cout << "✅ REST API Server started on port 8080!" << std::endl;
        std::cout << std::endl;
        
        std::cout << "📊 演进对比：" << std::endl;
        std::cout << "  Step 6: 同步执行 → 阻塞等待" << std::endl;
        std::cout << "  Step 7: 异步执行 → 立即返回 + 回调 + 并发" << std::endl;
        std::cout << std::endl;
        
        std::cout << "📡 Endpoints:" << std::endl;
        std::cout << "  POST /chat       - Mock LLM (无需配置) ✅" << std::endl;
        std::cout << "  POST /llm/chat   - Real LLM" 
                  << (real_llm_ready ? " ✅" : " ⚠️  (需 OPENAI_API_KEY)") << std::endl;
        std::cout << "  POST /batch      - 批量异步处理 ✅" << std::endl;
        std::cout << "  GET  /config     - 查看配置" << std::endl;
        std::cout << "  GET  /health     - 健康检查" << std::endl;
        std::cout << std::endl;
        
        std::cout << "🧪 Test:" << std::endl;
        std::cout << "  curl -X POST http://localhost:8080/chat \\" << std::endl;
        std::cout << "    -H 'Content-Type: application/json' \\" << std::endl;
        std::cout << "    -d '{\"message\":\"北京天气如何\"}'" << std::endl;
        std::cout << std::endl;
        
        std::cout << "  # 批量异步测试" << std::endl;
        std::cout << "  curl -X POST http://localhost:8080/batch \\" << std::endl;
        std::cout << "    -H 'Content-Type: application/json' \\" << std::endl;
        std::cout << "    -d '{\"messages\":[\"北京天气\",\"上海天气\",\"计算 2+3\"]}'" << std::endl;
        std::cout << std::endl;
        
        if (real_llm_ready) {
            std::cout << "  curl -X POST http://localhost:8080/llm/chat \\" << std::endl;
            std::cout << "    -H 'Content-Type: application/json' \\" << std::endl;
            std::cout << "    -d '{\"message\":\"你好\"}'" << std::endl;
            std::cout << std::endl;
        }
        
        std::cout << "📚 演进：" << std::endl;
        std::cout << "  Step 6 → 7: 同步 → 异步" << std::endl;
        std::cout << "  下一章：数据库持久化" << std::endl;
        std::cout << std::endl;
        
        io.run();
        
    } catch (const std::exception& e) {
        std::cerr << "[!] Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
