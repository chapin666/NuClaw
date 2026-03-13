// src/step04/main.cpp
// Step 4: Agent Loop - 执行引擎（循环检测、并行执行）
// 简化版核心代码结构

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

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
namespace json = boost::json;
using tcp = asio::ip::tcp;

// Tool 调用结构
struct ToolCall {
    std::string id;
    std::string name;
    json::value arguments;
};

struct ToolResult {
    std::string id;
    std::string output;
    bool success;
};

// Agent Loop 执行引擎
class AgentLoop {
public:
    enum class State { IDLE, THINKING, TOOL_CALLING, RESPONDING };
    
    struct Session {
        std::string id;
        State state = State::IDLE;
        std::vector<std::pair<std::string, std::string>> history;
        
        // 循环检测
        std::vector<std::string> recent_tool_calls;
        int loop_warning_count = 0;
    };
    
    std::string process(const std::string& input, const std::string& session_id) {
        auto& session = sessions_[session_id];
        session.id = session_id;
        
        session.state = State::THINKING;
        session.history.push_back({"user", input});
        
        // Step 4: 检测工具调用循环
        if (detect_loop(session)) {
            session.state = State::RESPONDING;
            return "Warning: Detected potential loop. Please try a different approach.";
        }
        
        // 模拟工具调用
        std::vector<ToolCall> calls = parse_tool_calls(input);
        
        if (!calls.empty()) {
            session.state = State::TOOL_CALLING;
            
            // Step 4: 并行执行工具
            std::vector<ToolResult> results = execute_tools_parallel(calls);
            
            // 记录工具调用用于循环检测
            for (const auto& call : calls) {
                session.recent_tool_calls.push_back(call.name + "_" + json::serialize(call.arguments));
                if (session.recent_tool_calls.size() > 10) {
                    session.recent_tool_calls.erase(session.recent_tool_calls.begin());
                }
            }
        }
        
        std::string response = "Processed: " + input + 
               " [tools: " + std::to_string(calls.size()) + 
               ", loop_checks: " + std::to_string(session.loop_warning_count) + "]";
        
        session.history.push_back({"assistant", response});
        session.state = State::IDLE;
        
        return response;
    }
    
private:
    bool detect_loop(Session& session) {
        // 简单循环检测：连续相同的工具调用
        if (session.recent_tool_calls.size() < 3) return false;
        
        auto& calls = session.recent_tool_calls;
        size_t n = calls.size();
        
        // 检查最后3次是否相同
        if (calls[n-1] == calls[n-2] && calls[n-2] == calls[n-3]) {
            session.loop_warning_count++;
            return true;
        }
        
        return false;
    }
    
    std::vector<ToolCall> parse_tool_calls(const std::string& input) {
        // 模拟解析工具调用
        std::vector<ToolCall> calls;
        if (input.find("/calc") != std::string::npos) {
            calls.push_back({"1", "calculator", json::object{{"expr", "1+1"}}});
        }
        if (input.find("/time") != std::string::npos) {
            calls.push_back({"2", "time", json::object{}});
        }
        return calls;
    }
    
    std::vector<ToolResult> execute_tools_parallel(const std::vector<ToolCall>& calls) {
        std::vector<std::future<ToolResult>> futures;
        
        for (const auto& call : calls) {
            futures.push_back(std::async(std::launch::async, [&call]() {
                // 模拟工具执行
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return ToolResult{call.id, "Result of " + call.name, true};
            }));
        }
        
        std::vector<ToolResult> results;
        for (auto& f : futures) {
            results.push_back(f.get());
        }
        
        return results;
    }
    
    std::map<std::string, Session> sessions_;
};

// WebSocket Session（简化版）
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
                std::string response = self->agent_.process(msg, "ws_session");
                self->ws_.text(true);
                self->ws_.async_write(asio::buffer(response),
                    [self](beast::error_code, std::size_t) { self->do_read(); });
            });
    }
    
    websocket::stream<tcp::socket> ws_;
    AgentLoop& agent_;
    beast::flat_buffer buffer_;
};

// 服务器
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

int main() {
    try {
        asio::io_context io;
        AgentLoop agent;
        Server server(io, 8081, agent);
        
        std::cout << "NuClaw Step 4 - Agent Execution Engine\n";
        std::cout << "Features: Loop detection, Parallel tool execution\n";
        std::cout << "WebSocket: ws://localhost:8081\n";
        
        std::vector<std::thread> threads;
        for (unsigned i = 0; i < std::thread::hardware_concurrency(); ++i) {
            threads.emplace_back([&io]() { io.run(); });
        }
        for (auto& t : threads) t.join();
        
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
