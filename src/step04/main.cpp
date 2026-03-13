// ============================================================================
// Step 4: Agent Loop - 执行引擎（循环检测、并行执行）
// ============================================================================
//
// 本章节重点：
//   1. 循环检测算法 - 防止 Agent 陷入无限循环
//   2. 并行工具执行 - 使用 std::async 提高性能
//   3. 工具调用框架 - 为 Step 6-8 打基础
//
// 代码结构：
//   ┌─────────────────────────────────────────────────────────────┐
//   │  main()                                                      │
//   │    ├── AgentLoop (执行引擎)                                  │
//   │    │     ├── process() - 主流程                             │
//   │    │     ├── detect_loop() - 循环检测                       │
//   │    │     └── execute_tools_parallel() - 并行执行            │
//   │    ├── WsSession (WebSocket 连接)                           │
//   │    └── Server (TCP 服务器)                                  │
//   └─────────────────────────────────────────────────────────────┘
//
// 编译: mkdir build && cd build && cmake .. && make
// 运行: ./nuclaw_step04
// 测试: wscat -c ws://localhost:8081
// ============================================================================

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <vector>
#include <map>
#include <memory>
#include <thread>
#include <future>
#include <functional>
#include <chrono>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
namespace json = boost::json;
using tcp = asio::ip::tcp;

// ----------------------------------------------------------------------------
// 数据结构：工具调用
// ----------------------------------------------------------------------------
// 为什么用结构体而不是直接用 JSON？
// - 类型安全：编译器会检查字段访问
// - 性能：避免重复的 JSON 解析
// - 清晰：代码自文档化
// ----------------------------------------------------------------------------
struct ToolCall {
    std::string id;           // 调用唯一标识（用于关联结果）
    std::string name;         // 工具名称（如 "calculator"）
    json::value arguments;    // 参数（JSON 对象）
};

struct ToolResult {
    std::string id;           // 对应 ToolCall 的 id
    std::string output;       // 工具输出
    bool success;             // 是否成功
};

// ----------------------------------------------------------------------------
// Agent Loop：执行引擎核心
// ----------------------------------------------------------------------------
// 职责：
//   1. 维护会话状态
//   2. 检测并防止循环调用
//   3. 并行执行工具
// ----------------------------------------------------------------------------
class AgentLoop {
public:
    // 状态定义
    enum class State { 
        IDLE,           // 空闲
        THINKING,       // 思考中
        TOOL_CALLING,   // 调用工具中
        RESPONDING      // 生成响应中
    };
    
    // 会话状态
    struct Session {
        std::string id;
        State state = State::IDLE;
        std::vector<std::pair<std::string, std::string>> history;
        
        // 循环检测相关
        std::vector<std::string> recent_tool_calls;  // 最近调用记录
        int loop_warning_count = 0;                    // 警告次数
    };
    
    // ---------------------------------------------------------------------
    // 主处理流程
    // ---------------------------------------------------------------------
    std::string process(const std::string& input, const std::string& session_id) {
        // 获取或创建会话
        auto& session = sessions_[session_id];
        session.id = session_id;
        
        // 记录用户输入
        session.history.push_back({"user", input});
        
        // 步骤 1: 进入思考状态
        session.state = State::THINKING;
        
        // 步骤 2: 检测循环（关键！）
        if (detect_loop(session)) {
            session.state = State::RESPONDING;
            session.history.push_back({"assistant", "Loop detected"});
            return "⚠️ Warning: Detected potential loop. Please try a different approach.";
        }
        
        // 步骤 3: 解析工具调用（简化版）
        // 实际应该由 LLM 生成工具调用 JSON
        std::vector<ToolCall> calls = parse_tool_calls(input);
        
        // 步骤 4: 执行工具
        if (!calls.empty()) {
            session.state = State::TOOL_CALLING;
            
            // 并行执行所有工具
            auto results = execute_tools_parallel(calls);
            
            // 记录调用历史（用于循环检测）
            for (const auto& call : calls) {
                // 生成调用签名：name + 参数
                std::string signature = call.name + "_" + json::serialize(call.arguments);
                session.recent_tool_calls.push_back(signature);
                
                // 只保留最近 10 次
                if (session.recent_tool_calls.size() > 10) {
                    session.recent_tool_calls.erase(session.recent_tool_calls.begin());
                }
            }
        }
        
        // 步骤 5: 生成响应
        session.state = State::RESPONDING;
        std::string response = "✓ Processed: " + input + 
               "\n   [tools: " + std::to_string(calls.size()) + 
               ", loop_checks: " + std::to_string(session.loop_warning_count) + "]";
        
        session.history.push_back({"assistant", response});
        session.state = State::IDLE;
        
        return response;
    }

private:
    // ---------------------------------------------------------------------
    // 循环检测算法
    // ---------------------------------------------------------------------
    // 策略：检测连续 3 次相同的工具调用
    // 优点：简单、高效
    // 缺点：对相似但不完全相同的调用无法检测
    // ---------------------------------------------------------------------
    bool detect_loop(Session& session) {
        const auto& calls = session.recent_tool_calls;
        
        // 至少需要 3 次调用才能检测
        if (calls.size() < 3) return false;
        
        size_t n = calls.size();
        
        // 检查最后 3 次是否完全相同
        if (calls[n-1] == calls[n-2] && calls[n-2] == calls[n-3]) {
            session.loop_warning_count++;
            std::cout << "[⚠️ Loop detected] Session: " << session.id 
                      << ", count: " << session.loop_warning_count << "\n";
            return true;
        }
        
        return false;
    }
    
    // ---------------------------------------------------------------------
    // 解析工具调用（简化版）
    // ---------------------------------------------------------------------
    // 实际项目中，这应该是 LLM 的输出解析器
    // 这里用字符串匹配模拟
    // ---------------------------------------------------------------------
    std::vector<ToolCall> parse_tool_calls(const std::string& input) {
        std::vector<ToolCall> calls;
        
        // 模拟解析：根据关键词创建工具调用
        if (input.find("/calc") != std::string::npos) {
            calls.push_back({"1", "calculator", json::object{{"expr", "1+1"}}});
        }
        if (input.find("/time") != std::string::npos) {
            calls.push_back({"2", "time", json::object{}});
        }
        if (input.find("/search") != std::string::npos) {
            calls.push_back({"3", "search", json::object{{"query", input}}});
        }
        
        return calls;
    }
    
    // ---------------------------------------------------------------------
    // 并行执行工具
    // ---------------------------------------------------------------------
    // 使用 std::async 实现并行执行
    // 特点：
    //   - 每个工具在新线程中执行
    //   - 使用 std::future 获取结果
    //   - 异常会传播到 future.get()
    // ---------------------------------------------------------------------
    std::vector<ToolResult> execute_tools_parallel(const std::vector<ToolCall>& calls) {
        std::vector<std::future<ToolResult>> futures;
        
        // 启动所有异步任务
        for (const auto& call : calls) {
            // std::launch::async 强制在新线程执行
            futures.push_back(
                std::async(std::launch::async, [&call]() -> ToolResult {
                    // 模拟工具执行时间
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    
                    std::cout << "[⚙️ Executing tool] " << call.name << "\n";
                    
                    return ToolResult{
                        call.id, 
                        "Result of " + call.name, 
                        true
                    };
                })
            );
        }
        
        // 收集所有结果
        std::vector<ToolResult> results;
        for (auto& f : futures) {
            results.push_back(f.get());  // 阻塞等待结果
        }
        
        return results;
    }
    
    std::map<std::string, Session> sessions_;
};

// ----------------------------------------------------------------------------
// WebSocket Session（简化版）
// ----------------------------------------------------------------------------
class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    WsSession(tcp::socket socket, AgentLoop& agent)
        : ws_(std::move(socket)), agent_(agent) {}
    
    void start() {
        ws_.async_accept(
            [self = shared_from_this()](beast::error_code ec) {
                if (!ec) self->do_read();
            });
    }

private:
    void do_read() {
        ws_.async_read(buffer_,
            [self = shared_from_this()](beast::error_code ec, std::size_t) {
                if (ec) return;
                
                std::string msg = beast::buffers_to_string(self->buffer_.data());
                self->buffer_.consume(self->buffer_.size());
                
                std::cout << "[<] " << msg << "\n";
                
                std::string response = self->agent_.process(msg, "ws_session");
                
                std::cout << "[>] " << response << "\n";
                
                self->ws_.text(true);
                self->ws_.async_write(asio::buffer(response),
                    [self](beast::error_code, std::size_t) { self->do_read(); });
            });
    }
    
    websocket::stream<tcp::socket> ws_;
    AgentLoop& agent_;
    beast::flat_buffer buffer_;
};

// ----------------------------------------------------------------------------
// TCP 服务器
// ----------------------------------------------------------------------------
class Server {
public:
    Server(asio::io_context& io, short port, AgentLoop& agent)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)), agent_(agent) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<WsSession>(std::move(socket), agent_)->start();
                }
                do_accept();
            });
    }
    
    tcp::acceptor acceptor_;
    AgentLoop& agent_;
};

// ----------------------------------------------------------------------------
// 主函数
// ----------------------------------------------------------------------------
int main() {
    try {
        std::cout << "========================================\n";
        std::cout << "  NuClaw Step 4 - Execution Engine\n";
        std::cout << "========================================\n\n";
        
        asio::io_context io;
        AgentLoop agent;
        Server server(io, 8081, agent);
        
        std::cout << "[✓] Server started on ws://localhost:8081\n\n";
        std::cout << "Features:\n";
        std::cout << "  • Loop detection (3 repeated calls)\n";
        std::cout << "  • Parallel tool execution\n\n";
        std::cout << "Test commands:\n";
        std::cout << "  /calc        - Trigger calculator tool\n";
        std::cout << "  /time        - Trigger time tool\n";
        std::cout << "  /calc x3     - Trigger loop detection\n\n";
        
        std::vector<std::thread> threads;
        for (unsigned i = 0; i < std::thread::hardware_concurrency(); ++i) {
            threads.emplace_back([&io]() { io.run(); });
        }
        for (auto& t : threads) t.join();
        
    } catch (std::exception& e) {
        std::cerr << "[✗] Error: " << e.what() << std::endl;
    }
    return 0;
}
