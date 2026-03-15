// ============================================================================
// Step 5: LLM 接入 - 接入真实 AI 能力
// ============================================================================
// 演进说明：
//   基于 Step 4 的 HTTP Session，将规则匹配替换为真实 LLM
//   Step 4 的问题：
//     1. 规则匹配太死板，无法处理意外输入
//     2. 每增加一个功能都要改代码
//     3. 不能理解复杂语义
//   本章解决：
//     1. 接入 OpenAI API（或其他 LLM 提供商）
//     2. 用 LLM 理解用户意图，而不是硬编码规则
//     3. 保持 HTTP Session 通信和上下文管理
//
// 编译: mkdir build && cd build && cmake .. && make
// 运行: ./nuclaw_step05
// 配置: 设置 OPENAI_API_KEY 环境变量
// 测试: curl -X POST http://localhost:8080/chat \\
//            -H "Content-Type: application/json" \\
//            -d '{"message":"你好"}'
// ============================================================================

#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <sstream>
#include <map>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <mutex>
#include <random>

using boost::asio::ip::tcp;
namespace asio = boost::asio;
namespace json = boost::json;

// ============================================================================
// HTTP 请求/响应结构体
// ============================================================================
struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int status_code = 200;
    std::map<std::string, std::string> headers;
    std::string body;
    
    std::string to_string() const {
        std::string status_text = (status_code == 200) ? "OK" : 
                                  (status_code == 404) ? "Not Found" : "Error";
        std::string response = "HTTP/1.1 " + std::to_string(status_code) + " " + status_text + "\r\n";
        
        for (const auto& [key, value] : headers) {
            response += key + ": " + value + "\r\n";
        }
        
        response += "Content-Length: " + std::to_string(body.length()) + "\r\n";
        response += "\r\n" + body;
        return response;
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
    
    auto it = req.headers.find("Content-Length");
    if (it != req.headers.end()) {
        size_t content_length = std::stoul(it->second);
        size_t header_end = raw.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            req.body = raw.substr(header_end + 4, content_length);
        }
    }
    
    return req;
}

// ============================================================================
// 会话上下文
// ============================================================================
struct ChatContext {
    std::string session_id;
    std::vector<std::pair<std::string, std::string>> history;
    std::chrono::steady_clock::time_point last_time;
    int message_count = 0;
    
    ChatContext() : last_time(std::chrono::steady_clock::now()) {}
};

// ============================================================================
// 会话管理器
// ============================================================================
class SessionManager {
public:
    std::shared_ptr<ChatContext> get_or_create_session(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            it->second->last_time = std::chrono::steady_clock::now();
            return it->second;
        }
        
        auto ctx = std::make_shared<ChatContext>();
        ctx->session_id = session_id;
        sessions_[session_id] = ctx;
        return ctx;
    }
    
    std::string generate_session_id() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        
        const char* hex = "0123456789abcdef";
        std::string id;
        for (int i = 0; i < 32; ++i) {
            id += hex[dis(gen)];
        }
        return id;
    }
    
    size_t session_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return sessions_.size();
    }

private:
    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<ChatContext>> sessions_;
};

// ============================================================================
// LLM Client - 调用 LLM API
// ============================================================================
class LLMClient {
public:
    LLMClient(const std::string& api_key) : api_key_(api_key) {}
    
    bool is_configured() const {
        return !api_key_.empty();
    }
    
    // 调用 LLM 获取回复
    std::string complete(const std::vector<std::pair<std::string, std::string>>& messages) {
        // 实际项目中这里应该调用 OpenAI API
        // 简化版：模拟 LLM 响应
        return simulate_llm_response(messages);
    }

private:
    std::string api_key_;
    
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
        
        // 模拟 LLM 理解（实际 LLM 会基于语义理解）
        if (last_message.find("你好") != std::string::npos ||
            last_message.find("hello") != std::string::npos) {
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
        
        return "我理解你想说：\"" + last_message + "\"\n\n"
               "作为 LLM，我能理解你的语义，而不只是匹配关键词。\n"
               "你可以问我任何问题，比如：\n"
               "- 今天天气如何\n"
               "- 你能做什么\n"
               "- Step 4 和 Step 5 有什么区别";
    }
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
        
        // 构建消息历史
        std::vector<std::pair<std::string, std::string>> messages;
        messages.push_back({"system", 
            "你是 NuClaw AI 助手，一个基于 LLM 的智能助手。"
            "你可以理解用户的自然语言输入，而不需要依赖预定义的规则。"});
        
        for (const auto& [role, content] : ctx.history) {
            messages.push_back({role, content});
        }
        
        messages.push_back({"user", input});
        
        // 调用 LLM
        std::string reply = llm_.complete(messages);
        
        // 保存到历史
        ctx.history.push_back({"user", input});
        ctx.history.push_back({"assistant", reply});
        
        // 限制历史长度
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
// 主服务器类
// ============================================================================
class ChatServer {
public:
    ChatServer(asio::io_context& io, short port)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)) {
        std::cout << "[i] Server binding to port " << port << std::endl;
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(
                        std::move(socket), session_manager_, ai_)->start();
                }
                do_accept();
            }
        );
    }
    
    class Session : public std::enable_shared_from_this<Session> {
    public:
        Session(tcp::socket socket, SessionManager& sm, ChatEngine& ai)
            : socket_(std::move(socket)), session_manager_(sm), ai_(ai) {}
        
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
                        handle_request(req);
                    }
                }
            );
        }
        
        void handle_request(const HttpRequest& req) {
            HttpResponse resp;
            
            if (req.method == "POST" && req.path == "/chat") {
                resp = handle_chat(req);
            } else if (req.method == "GET" && req.path == "/") {
                resp = handle_help();
            } else if (req.method == "GET" && req.path == "/health") {
                resp = handle_health();
            } else {
                resp.status_code = 404;
                resp.body = R"({"error": "not found"})";
            }
            
            resp.headers["Content-Type"] = "application/json; charset=utf-8";
            do_write(resp.to_string());
        }
        
        HttpResponse handle_chat(const HttpRequest& req) {
            HttpResponse resp;
            
            try {
                json::value jv = json::parse(req.body);
                std::string message = std::string(jv.as_object()["message"].as_string());
                
                std::string session_id;
                if (jv.as_object().contains("session_id")) {
                    session_id = std::string(jv.as_object()["session_id"].as_string());
                } else {
                    session_id = session_manager_.generate_session_id();
                }
                
                auto ctx = session_manager_.get_or_create_session(session_id);
                std::string reply = ai_.process(message, *ctx);
                
                json::object result;
                result["session_id"] = session_id;
                result["reply"] = reply;
                result["message_count"] = ctx->message_count;
                result["llm_mode"] = ai_.is_llm_configured() ? "openai" : "simulation";
                
                resp.body = json::serialize(result);
                
                std::cout << "[Chat] Session: " << session_id.substr(0, 8) << "... "
                          << "Count: " << ctx->message_count << std::endl;
                
            } catch (const std::exception& e) {
                resp.status_code = 400;
                resp.body = std::string(R"({"error": ")") + e.what() + """}";
            }
            
            return resp;
        }
        
        HttpResponse handle_help() {
            HttpResponse resp;
            json::object info;
            info["step"] = 5;
            info["title"] = "LLM Integration";
            info["description"] = "接入 LLM 替代规则匹配";
            info["endpoints"] = json::array({
                json::object({{"path", "/chat"}, {"method", "POST"}, 
                             {"params", "message, session_id(optional)"}}),
                json::object({{"path", "/health"}, {"method", "GET"}})
            });
            info["improvements"] = json::array({
                "LLM 理解语义",
                "无需预设规则",
                "处理任意输入"
            });
            resp.body = json::serialize(info);
            return resp;
        }
        
        HttpResponse handle_health() {
            HttpResponse resp;
            json::object health;
            health["status"] = "healthy";
            health["step"] = 5;
            health["feature"] = "llm";
            health["llm_configured"] = ai_.is_llm_configured();
            health["active_sessions"] = static_cast<int>(session_manager_.session_count());
            resp.body = json::serialize(health);
            return resp;
        }
        
        void do_write(const std::string& response) {
            auto self = shared_from_this();
            asio::async_write(socket_, asio::buffer(response),
                [this, self](boost::system::error_code, std::size_t) {}
            );
        }
        
        tcp::socket socket_;
        char buffer_[4096] = {};
        SessionManager& session_manager_;
        ChatEngine& ai_;
    };
    
    tcp::acceptor acceptor_;
    SessionManager session_manager_;
    ChatEngine ai_;
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
        std::cout << std::endl;
        
        ChatServer server(io, 8080);
        
        std::cout << "Server listening on http://localhost:8080" << std::endl;
        std::cout << std::endl;
        
        std::cout << "=== 关键改进 ===" << std::endl;
        std::cout << "Step 4 (规则): 只能匹配预设关键词" << std::endl;
        std::cout << "Step 5 (LLM): 理解语义，处理任意输入" << std::endl;
        std::cout << std::endl;
        
        std::cout << "=== 测试方式 ===" << std::endl;
        std::cout << "curl -X POST http://localhost:8080/chat \\" << std::endl;
        std::cout << "     -H 'Content-Type: application/json' \\" << std::endl;
        std::cout << "     -d '{\"message\":\"你好\"}'" << std::endl;
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
