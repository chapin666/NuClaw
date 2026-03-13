// ============================================================================
// Step 5: Agent Loop - 高级特性（记忆系统、上下文压缩、质量门控）
// ============================================================================
//
// 核心功能：
//   1. 长期记忆系统 - 存储和检索关键信息
//   2. 上下文压缩 - 解决 LLM 上下文窗口限制
//   3. 质量门控 - 自动评估和重试机制
//
// 设计要点：
//   - 短期记忆：对话历史（vector，速度快）
//   - 长期记忆：关键事实（带重要性评分）
//   - 压缩策略：超出 80% 阈值时触发摘要
//   - 质量评估：长度检查 + 关键词检查（简化版）
//
// ============================================================================

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
#include <algorithm>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
namespace json = boost::json;
using tcp = asio::ip::tcp;

// ----------------------------------------------------------------------------
// Agent Loop 高级版
// ----------------------------------------------------------------------------
class AgentLoop {
public:
    enum class State { IDLE, THINKING, TOOL_CALLING, RESPONDING };
    
    // ---------------------------------------------------------------------
    // Message: 对话消息（带 token 估算）
    // ---------------------------------------------------------------------
    struct Message {
        std::string role;      // "user" / "assistant" / "system"
        std::string content;
        size_t tokens = 0;     // 估算的 token 数（用于上下文管理）
    };
    
    // ---------------------------------------------------------------------
    // MemoryEntry: 长期记忆条目
    // ---------------------------------------------------------------------
    struct MemoryEntry {
        std::string key;       // 检索键
        std::string value;     // 内容
        float importance = 1.0; // 重要性分数（0-1，用于淘汰策略）
    };
    
    // ---------------------------------------------------------------------
    // Session: 会话状态
    // ---------------------------------------------------------------------
    struct Session {
        std::string id;
        State state = State::IDLE;
        std::vector<Message> history;
        std::vector<MemoryEntry> long_term_memory;  // 长期记忆存储
        
        // 上下文窗口管理
        size_t max_context_tokens = 4000;  // 最大 token 限制
        size_t current_tokens = 0;         // 当前已用 token
        
        // 质量评估状态
        int regenerate_count = 0;          // 重试次数
        float last_quality_score = 1.0;    // 上次质量评分
    };
    
    // ---------------------------------------------------------------------
    // 主处理流程
    // ---------------------------------------------------------------------
    std::string process(const std::string& input, const std::string& session_id) {
        auto& session = sessions_[session_id];
        session.id = session_id;
        session.state = State::THINKING;
        
        // 步骤 1: 检索相关记忆
        std::string context = retrieve_memory(session, input);
        
        // 步骤 2: 添加用户消息
        session.history.push_back({"user", input, estimate_tokens(input)});
        session.current_tokens += estimate_tokens(input);
        
        // 步骤 3: 上下文压缩（如果超过 80% 阈值）
        if (session.current_tokens > session.max_context_tokens * 0.8) {
            std::cout << "[📦 Compressing context] " << session.current_tokens 
                      << " -> " << (session.max_context_tokens * 0.5) << " tokens\n";
            compress_context(session);
        }
        
        // 步骤 4: 生成响应（带质量门控）
        std::string response = generate_with_quality_gate(input, context, session);
        
        // 步骤 5: 存储重要信息到长期记忆
        if (input.length() > 20) {
            store_memory(session, "pref_" + std::to_string(session.history.size()), input);
        }
        
        // 步骤 6: 添加助手响应
        session.history.push_back({"assistant", response, estimate_tokens(response)});
        session.current_tokens += estimate_tokens(response);
        session.state = State::IDLE;
        
        return response;
    }
    
    json::value get_session_info(const std::string& session_id) {
        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) {
            return json::object{{"error", "Session not found"}};
        }
        
        const auto& s = it->second;
        json::object info;
        info["session_id"] = session_id;
        info["state"] = static_cast<int>(s.state);
        info["context_tokens"] = static_cast<int>(s.current_tokens);
        info["max_tokens"] = static_cast<int>(s.max_context_tokens);
        info["memory_entries"] = static_cast<int>(s.long_term_memory.size());
        info["history_size"] = static_cast<int>(s.history.size());
        info["quality_score"] = s.last_quality_score;
        return info;
    }

private:
    // Token 估算（简化：英文约 4 字符/token）
    size_t estimate_tokens(const std::string& text) {
        return text.length() / 4 + 1;
    }
    
    // 记忆检索：返回重要性 > 0.5 的记忆
    std::string retrieve_memory(Session& session, const std::string& query) {
        std::string result;
        for (const auto& mem : session.long_term_memory) {
            if (mem.importance > 0.5) {
                result += mem.key + ": " + mem.value + "; ";
            }
        }
        return result.empty() ? "[no relevant memory]" : result;
    }
    
    // 存储记忆（带容量限制和淘汰策略）
    void store_memory(Session& session, const std::string& key, const std::string& value) {
        // 容量限制：最多 50 条
        if (session.long_term_memory.size() >= 50) {
            // 淘汰策略：移除重要性最低的
            auto min_it = std::min_element(session.long_term_memory.begin(),
                session.long_term_memory.end(),
                [](const MemoryEntry& a, const MemoryEntry& b) {
                    return a.importance < b.importance;
                });
            session.long_term_memory.erase(min_it);
        }
        session.long_term_memory.push_back({key, value, 1.0f});
    }
    
    // 上下文压缩：生成摘要，保留最近消息
    void compress_context(Session& session) {
        if (session.history.size() < 10) return;
        
        // 保留最近 5 条
        std::vector<Message> recent(session.history.end() - 5, session.history.end());
        
        // 前面的生成摘要（简化版）
        std::string summary = "[Summary of " + 
            std::to_string(session.history.size() - 5) + " messages]";
        
        // 替换历史
        session.history = {
            {"system", summary, estimate_tokens(summary)}
        };
        session.history.insert(session.history.end(), recent.begin(), recent.end());
        
        // 重新计算 token
        session.current_tokens = 0;
        for (const auto& m : session.history) {
            session.current_tokens += m.tokens;
        }
    }
    
    // 质量评估（简化规则版）
    float evaluate_quality(const std::string& response) {
        if (response.length() < 10) return 0.3f;  // 太短
        if (response.find("error") != std::string::npos) return 0.5f;  // 含错误
        if (response.length() > 100) return 0.95f;  // 详细回复
        return 0.8f;
    }
    
    // 带质量门控的生成
    std::string generate_with_quality_gate(const std::string& input, 
                                           const std::string& context,
                                           Session& session) {
        std::string response = "📝 Response to: " + input + 
                             "\n   [context: " + context + 
                             ", tokens: " + std::to_string(session.current_tokens) + "]";
        
        float quality = evaluate_quality(response);
        session.last_quality_score = quality;
        
        // 质量不合格且重试次数 < 3，则重试
        if (quality < 0.7f && session.regenerate_count < 3) {
            session.regenerate_count++;
            std::cout << "[🔄 Regenerating] Attempt " << session.regenerate_count 
                      << " (quality: " << quality << ")\n";
            return generate_with_quality_gate(input, context, session);
        }
        
        session.regenerate_count = 0;
        return response;
    }
    
    std::map<std::string, Session> sessions_;
};

// ----------------------------------------------------------------------------
// WebSocket Session
// ----------------------------------------------------------------------------
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
            
            std::cout << "[<] " << msg << "\n";
            std::string response = self->agent_.process(msg, "ws_" + std::to_string(
                reinterpret_cast<uintptr_t>(self.get())));
            std::cout << "[>] " << response.substr(0, 100) << "...\n";
            
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
// Server
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------------
int main() {
    try {
        std::cout << "========================================\n";
        std::cout << "  NuClaw Step 5 - Advanced Features\n";
        std::cout << "========================================\n\n";
        
        asio::io_context io;
        AgentLoop agent;
        Server server(io, 8081, agent);
        
        std::cout << "[✓] Server started on ws://localhost:8081\n\n";
        std::cout << "Features:\n";
        std::cout << "  • Long-term memory (importance-based)\n";
        std::cout << "  • Context compression (80% threshold)\n";
        std::cout << "  • Quality gate (auto-retry)\n\n";
        std::cout << "Send long messages (>20 chars) to store in memory\n\n";
        
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
