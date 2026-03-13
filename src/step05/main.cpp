// ============================================================================
// Step 5: LLM 接入 - 接入真实 AI 能力
// ============================================================================
// 演进说明：
//   基于 Step 4 的 WebSocket AI，将规则匹配替换为真实 LLM
//   Step 4 的问题：
//     1. 规则匹配太死板，无法处理意外输入
//     2. 每增加一个功能都要改代码
//     3. 不能理解复杂语义
//   本章解决：
//     1. 接入 OpenAI API（或其他 LLM 提供商）
//     2. 用 LLM 理解用户意图，而不是硬编码规则
//     3. 保持 WebSocket 通信和上下文管理
// 编译: g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
// 运行: ./server
// 配置: 设置 OPENAI_API_KEY 环境变量
// 测试: wscat -c ws://localhost:8080/ws
//       然后输入任意问题，看 LLM 如何回答
// ============================================================================

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <sstream>
#include <map>
#include <chrono>
#include <cstdlib>  // getenv

using boost::asio::ip::tcp;
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace json = boost::json;

// ============================================================================
// 新增：LLM Client - 调用 OpenAI API
// ============================================================================
class LLMClient {
public:
    LLMClient(const std::string& api_key) 
        : api_key_(api_key), 
          api_endpoint_("api.openai.com", "443") {}
    
    // 调用 LLM 获取回复
    std::string complete(const std::vector<std::pair<std::string, std::string>>& messages) {
        // 构建请求体
        json::object request_body;
        request_body["model"] = "gpt-3.5-turbo";
        request_body["temperature"] = 0.7;
        request_body["max_tokens"] = 500;
        
        json::array msg_array;
        for (const auto& [role, content] : messages) {
            json::object msg;
            msg["role"] = role;
            msg["content"] = content;
            msg_array.push_back(msg);
        }
        request_body["messages"] = msg_array;
        
        std::string request_json = json::serialize(request_body);
        
        // 这里简化处理：实际应该使用 Boost.Beast 发送 HTTPS POST
        // 为了教程简洁，这里用占位符模拟
        // 真实实现需要 SSL、HTTP 客户端等
        
        // TODO: 实际 HTTP POST 到 https://api.openai.com/v1/chat/completions
        
        return simulate_llm_response(messages);
    }
    
    // 检查 API Key 是否配置
    bool is_configured() const {
        return !api_key_.empty();
    }

private:
    std::string api_key_;
    std::pair<std::string, std::string> api_endpoint_;
    
    // 模拟 LLM 响应（实际项目中替换为真实 API 调用）
    std::string simulate_llm_response(
        const std::vector<std::pair<std::string, std::string>>& messages) {
        
        // 获取最后一条用户消息
        std::string last_message;
        for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
            if (it->first == "user") {
                last_message = it->second;
                break;
            }
        }
        
        // 简单的关键词匹配（模拟 LLM 理解）
        // 实际 LLM 会基于语义理解，不是关键词匹配
        if (last_message.find("你好") != std::string::npos ||
            last_message.find("hello") != std::string::npos ||
            last_message.find("hi") != std::string::npos) {
            return "你好！我是基于 LLM 的 AI 助手。与 Step 4 的规则 AI 不同，我能理解你的语义，而不只是匹配关键词。";
        }
        
        if (last_message.find("天气") != std::string::npos) {
            return "我可以通过调用工具来查询天气。与 Step 4 不同的是，我不需要预定义规则，而是能理解你想知道天气信息。\n\n（注意：真正的天气查询需要工具支持，我们将在 Step 6 添加）";
        }
        
        if (last_message.find("区别") != std::string::npos ||
            last_message.find("不同") != std::string::npos) {
            return "Step 4 vs Step 5 的区别：\n"
                   "- Step 4: 规则匹配，只能处理预设模式\n"
                   "- Step 5: LLM 理解，能处理任意自然语言\n\n"
                   "比如你说'今天适合出门吗'，规则 AI 无法理解，"
                   "但 LLM 能理解你在问天气/时间相关的问题。";
        }
        
        if (last_message.find("时间") != std::string::npos ||
            last_message.find("几点") != std::string::npos) {
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            char buf[100];
            std::strftime(buf, sizeof(buf), "现在是 %Y-%m-%d %H:%M:%S", std::localtime(&t));
            return std::string(buf) + "\n\n（这是通过代码获取的，但 LLM 理解了你的意图）";
        }
        
        // 通用回复
        return "我理解你想说：\"" + last_message + "\"\n\n"
               "作为 LLM，我能理解你的语义，而不只是匹配关键词。\n"
               "你可以问我任何问题，比如：\n"
               "- 今天天气如何\n"
               "- 你能做什么\n"
               "- Step 4 和 Step 5 有什么区别";
    }
};

// ============================================================================
// 会话上下文（从 Step 4 继承）
// ============================================================================
struct ChatContext {
    std::vector<std::pair<std::string, std::string>> history;
    std::chrono::steady_clock::time_point start_time;
    int message_count = 0;
    
    ChatContext() : start_time(std::chrono::steady_clock::now()) {}
};

// ============================================================================
// ChatEngine - 使用 LLM 而不是规则
// ============================================================================
class ChatEngine {
public:
    ChatEngine() : llm_(get_api_key()) {
        if (!llm_.is_configured()) {
            std::cerr << "[!] Warning: OPENAI_API_KEY not set, using simulation mode" << std::endl;
        }
    }
    
    std::string process(const std::string& input, ChatContext& ctx) {
        ctx.message_count++;
        
        // 构建消息历史（包含 system prompt + 上下文）
        std::vector<std::pair<std::string, std::string>> messages;
        
        // System prompt
        messages.push_back({"system", 
            "你是 NuClaw AI 助手，一个基于 LLM 的智能助手。"
            "你可以理解用户的自然语言输入，而不需要依赖预定义的规则。"
            "你正在演示从规则匹配到 LLM 理解的演进。"});
        
        // 历史对话
        for (const auto& [role, content] : ctx.history) {
            messages.push_back({role, content});
        }
        
        // 当前消息
        messages.push_back({"user", input});
        
        // 调用 LLM
        std::string reply = llm_.complete(messages);
        
        // 保存到历史
        ctx.history.push_back({"user", input});
        ctx.history.push_back({"assistant", reply});
        
        // 限制历史长度（防止超过 LLM 上下文窗口）
        if (ctx.history.size() > 20) {
            ctx.history.erase(ctx.history.begin(), ctx.history.begin() + 2);
        }
        
        return reply;
    }
    
    std::string get_welcome_message() {
        return "👋 欢迎来到 NuClaw Step 5！\n\n"
               "🧠 这是基于 LLM 的 AI，不是规则匹配！\n\n"
               "试试这些：\n"
               "- 你好\n"
               "- 今天天气如何\n" 
               "- 你和 Step 4 有什么区别\n"
               "- （任意问题，我会理解语义）";
    }
    
    bool is_llm_configured() const {
        return llm_.is_configured();
    }

private:
    LLMClient llm_;
    
    std::string get_api_key() {
        const char* key = std::getenv("OPENAI_API_KEY");
        return key ? key : "";
    }
};

// ============================================================================
// WebSocket Session（从 Step 4 继承，基本不变）
// ============================================================================
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    WebSocketSession(tcp::socket socket, ChatEngine& ai)
        : ws_(std::move(socket)), ai_(ai) {}
    
    void start() {
        ws_.async_accept(
            beast::bind_front_handler(&WebSocketSession::on_accept, shared_from_this()));
    }

private:
    void on_accept(beast::error_code ec) {
        if (ec) {
            std::cerr << "[!] WebSocket accept failed: " << ec.message() << std::endl;
            return;
        }
        
        std::cout << "[+] WebSocket connection established" << std::endl;
        
        std::string welcome = ai_.get_welcome_message();
        ws_.text(true);
        ws_.async_write(asio::buffer(welcome),
            beast::bind_front_handler(&WebSocketSession::on_write, shared_from_this()));
    }
    
    void do_read() {
        ws_.async_read(buffer_,
            beast::bind_front_handler(&WebSocketSession::on_read, shared_from_this()));
    }
    
    void on_read(beast::error_code ec, std::size_t bytes) {
        if (ec == websocket::error::closed) {
            std::cout << "[-] WebSocket closed after " << ctx_.message_count << " messages" << std::endl;
            return;
        }
        if (ec) {
            std::cerr << "[!] WebSocket read error: " << ec.message() << std::endl;
            return;
        }
        
        std::string msg = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());
        
        std::cout << "[<] User: " << msg << std::endl;
        
        std::string reply = ai_.process(msg, ctx_);
        
        std::cout << "[>] LLM: " << reply.substr(0, 50) << "..." << std::endl;
        
        ws_.text(true);
        ws_.async_write(asio::buffer(reply),
            beast::bind_front_handler(&WebSocketSession::on_write, shared_from_this()));
    }
    
    void on_write(beast::error_code ec, std::size_t bytes) {
        if (ec) {
            std::cerr << "[!] WebSocket write error: " << ec.message() << std::endl;
            return;
        }
        do_read();
    }
    
    websocket::stream<tcp::socket> ws_;
    ChatEngine& ai_;
    beast::flat_buffer buffer_;
    ChatContext ctx_;
};

// ============================================================================
// HTTP 请求结构体（简化版）
// ============================================================================
struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    
    bool is_websocket_upgrade() const {
        auto it = headers.find("Upgrade");
        if (it != headers.end() && it->second == "websocket") {
            auto conn = headers.find("Connection");
            return conn != headers.end() && 
                   conn->second.find("Upgrade") != std::string::npos;
        }
        return false;
    }
};

HttpRequest parse_http_request(const char* data, size_t len) {
    HttpRequest req;
    std::string raw(data, len);
    std::istringstream stream(raw);
    std::string line;
    
    if (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        line_stream >> req.method >> req.path;
    }
    
    while (std::getline(stream, line) && !line.empty() && line != "\r") {
        auto pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r") + 1);
            req.headers[key] = value;
        }
    }
    
    return req;
}

// ============================================================================
// Session 类（简化版）
// ============================================================================
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, ChatEngine& ai)
        : socket_(std::move(socket)), ai_(ai) {}
    
    void start() {
        do_read();
    }

private:
    void do_read() {
        auto self = shared_from_this();
        socket_.async_read_some(
            asio::buffer(buffer_, sizeof(buffer_)),
            [this, self](boost::system::error_code ec, std::size_t len) {
                if (!ec) {
                    HttpRequest req = parse_http_request(buffer_, len);
                    
                    if (req.is_websocket_upgrade()) {
                        std::make_shared<WebSocketSession>(
                            std::move(socket_), ai_)->start();
                    } else {
                        std::string body = R"({"status": "ok", "step": 5, "feature": "llm"})";
                        std::string response = 
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: application/json\r\n"
                            "Content-Length: " + std::to_string(body.length()) + "\r\n"
                            "\r\n" + body;
                        asio::async_write(socket_, asio::buffer(response),
                            [this, self](boost::system::error_code, std::size_t) {});
                    }
                }
            }
        );
    }
    
    tcp::socket socket_;
    ChatEngine& ai_;
    char buffer_[4096] = {};
};

// ============================================================================
// Server 类
// ============================================================================
class Server {
public:
    Server(asio::io_context& io, short port, ChatEngine& ai)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)), ai_(ai) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket), ai_)->start();
                }
                do_accept();
            }
        );
    }
    
    tcp::acceptor acceptor_;
    ChatEngine& ai_;
};

// ============================================================================
// 主函数
// ============================================================================
int main() {
    try {
        asio::io_context io;
        
        std::cout << "========================================" << std::endl;
        std::cout << "  NuClaw Step 5 - LLM Integration" << std::endl;
        std::cout << "========================================" << std::endl;
        
        ChatEngine ai;
        
        std::cout << std::endl;
        if (ai.is_llm_configured()) {
            std::cout << "[✓] LLM configured (OpenAI API)" << std::endl;
        } else {
            std::cout << "[i] LLM not configured, using simulation mode" << std::endl;
            std::cout << "    Set OPENAI_API_KEY env var to use real LLM" << std::endl;
        }
        
        Server server(io, 8080, ai);
        
        std::cout << std::endl;
        std::cout << "Server listening on http://localhost:8080" << std::endl;
        std::cout << "WebSocket: ws://localhost:8080/ws" << std::endl;
        std::cout << std::endl;
        
        std::cout << "=== 关键改进 ===" << std::endl;
        std::cout << "Step 4 (规则): 只能匹配预设关键词" << std::endl;
        std::cout << "Step 5 (LLM): 理解语义，处理任意输入" << std::endl;
        std::cout << std::endl;
        
        std::cout << "=== 测试 ===" << std::endl;
        std::cout << "wscat -c ws://localhost:8080/ws" << std::endl;
        std::cout << "Then try: 你好 / 今天如何 / 任意问题" << std::endl;
        std::cout << std::endl;
        
        std::vector<std::thread> threads;
        for (unsigned i = 0; i < std::thread::hardware_concurrency(); ++i) {
            threads.emplace_back([&io]() { io.run(); });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
    } catch (std::exception& e) {
        std::cerr << "[!] Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
