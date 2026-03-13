// src/step03/main.cpp
// Step 3: Agent Loop - 状态机与对话管理
// 编译: mkdir build && cd build && cmake .. && make
// 运行: ./nuclaw_step03

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <thread>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
namespace json = boost::json;
using tcp = asio::ip::tcp;

// Agent Loop 核心
class AgentLoop {
public:
    enum class State {
        IDLE,           // 空闲
        THINKING,       // 思考中
        TOOL_CALLING,   // 调用工具
        RESPONDING      // 响应中
    };
    
    struct Message {
        std::string role;      // user, assistant, system
        std::string content;
        std::string timestamp;
    };
    
    struct Session {
        std::string id;
        State state = State::IDLE;
        std::vector<Message> history;
        size_t max_history = 100;  // 最大历史记录数
        
        void add_message(const std::string& role, const std::string& content) {
            if (history.size() >= max_history) {
                history.erase(history.begin());  // 移除最旧的
            }
            history.push_back({role, content, current_time()});
        }
        
        std::string current_time() {
            auto now = std::time(nullptr);
            char buf[100];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
            return buf;
        }
    };
    
    std::string process(const std::string& user_input, const std::string& session_id) {
        auto& session = sessions_[session_id];
        session.id = session_id;
        
        // 状态转换: IDLE -> THINKING
        session.state = State::THINKING;
        
        // 添加用户消息
        session.add_message("user", user_input);
        
        // 生成响应（简化版 echo）
        std::string response = generate_response(user_input, session);
        
        // 添加助手消息
        session.add_message("assistant", response);
        
        // 状态转换: THINKING -> IDLE
        session.state = State::IDLE;
        
        return response;
    }
    
    json::value get_session_info(const std::string& session_id) {
        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) {
            return json::object{{"error", "session not found"}};
        }
        
        json::object info;
        info["session_id"] = session_id;
        info["state"] = state_to_string(it->second.state);
        info["message_count"] = static_cast<int>(it->second.history.size());
        info["max_history"] = static_cast<int>(it->second.max_history);
        
        json::array msgs;
        for (const auto& m : it->second.history) {
            json::object msg;
            msg["role"] = m.role;
            msg["content"] = m.content;
            msg["timestamp"] = m.timestamp;
            msgs.push_back(msg);
        }
        info["history"] = msgs;
        
        return info;
    }
    
private:
    std::string generate_response(const std::string& input, Session& session) {
        // Step 3: 简单的 echo，带状态信息
        return "Echo: " + input + 
               " [state: " + state_to_string(session.state) + 
               ", history: " + std::to_string(session.history.size()) + " msgs]";
    }
    
    std::string state_to_string(State s) {
        switch (s) {
            case State::IDLE: return "idle";
            case State::THINKING: return "thinking";
            case State::TOOL_CALLING: return "tool_calling";
            case State::RESPONDING: return "responding";
            default: return "unknown";
        }
    }
    
    std::map<std::string, Session> sessions_;
};

// WebSocket Session
class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    WsSession(tcp::socket socket, AgentLoop& agent)
        : ws_(std::move(socket)), agent_(agent) {}
    
    void start() {
        ws_.async_accept(
            beast::bind_front_handler(&WsSession::on_accept, shared_from_this()));
    }
    
private:
    void on_accept(beast::error_code ec) {
        if (ec) return;
        session_id_ = "ws_" + std::to_string(reinterpret_cast<uintptr_t>(this));
        do_read();
    }
    
    void do_read() {
        ws_.async_read(buffer_,
            beast::bind_front_handler(&WsSession::on_read, shared_from_this()));
    }
    
    void on_read(beast::error_code ec, std::size_t) {
        if (ec) return;
        
        std::string msg = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());
        
        std::string response = agent_.process(msg, session_id_);
        
        ws_.text(true);
        ws_.async_write(asio::buffer(response),
            beast::bind_front_handler(&WsSession::on_write, shared_from_this()));
    }
    
    void on_write(beast::error_code ec, std::size_t) {
        if (ec) return;
        do_read();
    }
    
    websocket::stream<tcp::socket> ws_;
    AgentLoop& agent_;
    beast::flat_buffer buffer_;
    std::string session_id_;
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
        
        std::cout << "NuClaw Step 3 - Agent Loop\n";
        std::cout << "WebSocket: ws://localhost:8081\n";
        
        std::vector<std::thread> threads;
        for (unsigned i = 0; i < std::thread::hardware_concurrency(); ++i) {
            threads.emplace_back([&io]() { io.run(); });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
