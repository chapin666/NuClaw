// src/step05/main.cpp
// Step 5: Agent Loop - 高级特性（记忆系统、摘要、质量门控）

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <vector>
#include <map>
#include <memory>
#include <thread>
#include <sstream>
#include <queue>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
namespace json = boost::json;
using tcp = asio::ip::tcp;

// Agent Loop 高级版
class AgentLoop {
public:
    enum class State { IDLE, THINKING, TOOL_CALLING, RESPONDING };
    
    struct Message {
        std::string role;
        std::string content;
        size_t tokens = 0;  // 估算的 token 数
    };
    
    // 记忆条目
    struct MemoryEntry {
        std::string key;
        std::string value;
        float importance = 1.0;  // 重要性分数
    };
    
    struct Session {
        std::string id;
        State state = State::IDLE;
        std::vector<Message> history;
        std::vector<MemoryEntry> long_term_memory;  // 长期记忆
        
        // 上下文窗口管理
        size_t max_context_tokens = 4000;
        size_t current_tokens = 0;
        
        // 质量评估
        int regenerate_count = 0;
        float last_quality_score = 1.0;
    };
    
    std::string process(const std::string& input, const std::string& session_id) {
        auto& session = sessions_[session_id];
        session.id = session_id;
        session.state = State::THINKING;
        
        // 添加上下文记忆
        std::string context = retrieve_memory(session, input);
        
        session.history.push_back({"user", input, estimate_tokens(input)});
        session.current_tokens += estimate_tokens(input);
        
        // 上下文窗口管理
        if (session.current_tokens > session.max_context_tokens * 0.8) {
            compress_context(session);
        }
        
        // 生成响应
        std::string response = generate_with_quality_gate(input, context, session);
        
        // 存储重要信息到长期记忆
        if (input.length() > 20) {
            store_memory(session, "user_pref_" + std::to_string(session.history.size()), input);
        }
        
        session.history.push_back({"assistant", response, estimate_tokens(response)});
        session.current_tokens += estimate_tokens(response);
        session.state = State::IDLE;
        
        return response;
    }
    
    json::value get_session_info(const std::string& session_id) {
        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) {
            return json::object{{"error", "not found"}};
        }
        
        json::object info;
        info["session_id"] = session_id;
        info["state"] = static_cast<int>(it->second.state);
        info["context_tokens"] = static_cast<int>(it->second.current_tokens);
        info["max_tokens"] = static_cast<int>(it->second.max_context_tokens);
        info["memory_count"] = static_cast<int>(it->second.long_term_memory.size());
        info["regenerate_count"] = it->second.regenerate_count;
        return info;
    }
    
private:
    size_t estimate_tokens(const std::string& text) {
        // 简单估算：约 4 字符 1 个 token
        return text.length() / 4;
    }
    
    std::string retrieve_memory(Session& session, const std::string& query) {
        // 简单记忆检索：返回最近的重要记忆
        std::string result;
        for (const auto& mem : session.long_term_memory) {
            if (mem.importance > 0.5) {
                result += mem.key + ": " + mem.value + "; ";
            }
        }
        return result.empty() ? "no_memory" : result;
    }
    
    void store_memory(Session& session, const std::string& key, const std::string& value) {
        if (session.long_term_memory.size() > 50) {
            // 移除最不重要的一条
            auto min_it = std::min_element(session.long_term_memory.begin(),
                session.long_term_memory.end(),
                [](const MemoryEntry& a, const MemoryEntry& b) {
                    return a.importance < b.importance;
                });
            session.long_term_memory.erase(min_it);
        }
        session.long_term_memory.push_back({key, value, 1.0f});
    }
    
    void compress_context(Session& session) {
        // 摘要生成：移除旧消息，保留关键信息
        if (session.history.size() < 10) return;
        
        // 保留最近 5 条，前面的生成摘要
        std::string summary = "Summary of " + std::to_string(session.history.size() - 5) + " messages";
        
        // 清空旧历史
        session.current_tokens = 0;
        for (const auto& m : session.history) {
            session.current_tokens += m.tokens;
        }
        
        // 替换为摘要
        session.history = {
            {"system", summary, estimate_tokens(summary)},
        };
        // 保留最近的消息...
        session.current_tokens = estimate_tokens(summary);
    }
    
    float evaluate_quality(const std::string& response) {
        // 简单质量评估
        if (response.length() < 10) return 0.3f;
        if (response.find("error") != std::string::npos) return 0.5f;
        return 0.9f;
    }
    
    std::string generate_with_quality_gate(const std::string& input, 
                                           const std::string& context,
                                           Session& session) {
        std::string response = "Response to: " + input + 
                             " [context: " + context + 
                             ", tokens: " + std::to_string(session.current_tokens) + "]";
        
        // 质量门控
        float quality = evaluate_quality(response);
        session.last_quality_score = quality;
        
        if (quality < 0.7 && session.regenerate_count < 3) {
            session.regenerate_count++;
            return generate_with_quality_gate(input, context, session);  // 重试
        }
        
        session.regenerate_count = 0;
        return response;
    }
    
    std::map<std::string, Session> sessions_;
};

// WebSocket Session
class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    WsSession(tcp::socket socket, AgentLoop& agent)
        : ws_(std::move(socket)), agent_(agent) {}
    
    void start() {
        ws_.async_accept([self = shared_from_this()](beast::error_code ec) {
            if (!ec) self->do_read();
        });
    }
    
private:
    void do_read() {
        ws_.async_read(buffer_, [self = shared_from_this()](beast::error_code ec, std::size_t) {
            if (ec) return;
            std::string msg = beast::buffers_to_string(self->buffer_.data());
            self->buffer_.consume(self->buffer_.size());
            std::string response = self->agent_.process(msg, "ws_" + std::to_string(
                reinterpret_cast<uintptr_t>(self.get())));
            self->ws_.text(true);
            self->ws_.async_write(asio::buffer(response),
                [self](beast::error_code, std::size_t) { self->do_read(); });
        });
    }
    
    websocket::stream<tcp::socket> ws_;
    AgentLoop& agent_;
    beast::flat_buffer buffer_;
};

class Server {
public:
    Server(asio::io_context& io, short port, AgentLoop& agent)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)), agent_(agent) {
        do_accept();
    }
    
private:
    void do_accept() {
        acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) std::make_shared<WsSession>(std::move(socket), agent_)->start();
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
        
        std::cout << "NuClaw Step 5 - Agent Advanced Features\n";
        std::cout << "Features: Memory, Context compression, Quality gate\n";
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
