// ============================================================================
// Step 4: HTTP 会话管理 - 解决无状态问题
// ============================================================================
// 演进说明：
//   基于 Step 3 的规则 AI，解决 HTTP 无状态问题
//   Step 3 的问题：
//     1. 每次请求都是新的 HTTP 连接
//     2. 上下文（ChatContext）无法持久化
//     3. 用户问"北京天气"后再问"那上海呢"，AI 无法理解
//   本章解决：
//     1. 用 Session ID 标识会话
//     2. 服务端维护会话上下文（内存存储）
//     3. 支持上下文关联提问
//
// 编译: g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
// 运行: ./server
// 测试: curl -X POST http://localhost:8080/chat \\
//            -H "Content-Type: application/json" \\
//            -d '{"message":"北京天气"}'
//       # 记录返回的 session_id
//       curl -X POST http://localhost:8080/chat \\
//            -H "Content-Type: application/json" \\
//            -d '{"session_id":"xxx","message":"那上海呢"}'
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
#include <functional>
#include <regex>
#include <chrono>
#include <ctime>
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

// ============================================================================
// HTTP 请求解析
// ============================================================================
HttpRequest parse_http_request(const char* data, size_t len) {
    HttpRequest req;
    std::string raw(data, len);
    std::istringstream stream(raw);
    std::string line;
    
    // 解析请求行
    if (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        line_stream >> req.method >> req.path;
    }
    
    // 解析头部
    while (std::getline(stream, line) && !line.empty() && line != "\r") {
        auto pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            // trim
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r") + 1);
            req.headers[key] = value;
        }
    }
    
    // 解析 body（如果有 Content-Length）
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
    std::string last_intent;
    std::string last_topic;
    std::chrono::steady_clock::time_point last_time;
    int message_count = 0;
    std::vector<std::pair<std::string, std::string>> history;  // 对话历史
    
    // 检查上下文是否有效（30秒内）
    bool is_valid() const {
        if (last_intent.empty()) return false;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_time).count();
        return elapsed < 30;
    }
};

// ============================================================================
// 会话管理器 - 核心改进！
// ============================================================================
class SessionManager {
public:
    // 获取或创建会话
    std::shared_ptr<ChatContext> get_or_create_session(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = sessions_.find(session_id);
        if (it != sessions_.end()) {
            return it->second;
        }
        
        // 创建新会话
        auto ctx = std::make_shared<ChatContext>();
        ctx->session_id = session_id;
        sessions_[session_id] = ctx;
        return ctx;
    }
    
    // 生成新的 Session ID
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
    
    // 清理过期会话
    void cleanup_expired() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        
        for (auto it = sessions_.begin(); it != sessions_.end();) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second->last_time).count();
            if (elapsed > 300) {  // 5分钟过期
                it = sessions_.erase(it);
            } else {
                ++it;
            }
        }
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
// ChatEngine（从 Step 3 继承，但现在利用会话上下文）
// ============================================================================
class ChatEngine {
public:
    ChatEngine() {
        init_rules();
    }
    
    std::string process(const std::string& input, ChatContext& ctx) {
        ctx.message_count++;
        
        // 记录历史
        ctx.history.push_back({"user", input});
        
        // 检查是否是上下文相关的提问
        if (is_contextual_question(input, ctx)) {
            std::string reply = handle_contextual(input, ctx);
            ctx.last_time = std::chrono::steady_clock::now();
            ctx.history.push_back({"ai", reply});
            return reply;
        }
        
        // 规则匹配
        for (const auto& rule : rules_) {
            if (std::regex_search(input, rule.pattern)) {
                ctx.last_intent = rule.intent;
                ctx.last_topic = extract_topic(input, rule.intent);
                ctx.last_time = std::chrono::steady_clock::now();
                std::string reply = rule.response(input);
                ctx.history.push_back({"ai", reply});
                return reply;
            }
        }
        
        std::string fallback = "我不理解你的问题，可以试试：你好、时间、北京天气、那上海呢";
        ctx.history.push_back({"ai", fallback});
        return fallback;
    }
    
    std::string get_welcome_message() {
        return "👋 欢迎来到 NuClaw AI！\n"
               "我会记住你的对话，可以问上下文相关的问题。\n"
               "试试：北京天气 → 那上海呢";
    }

private:
    struct Rule {
        std::regex pattern;
        std::string intent;
        std::function<std::string(const std::string&)> response;
    };
    
    std::vector<Rule> rules_;
    
    void init_rules() {
        rules_.push_back({
            std::regex("你好|嗨|hello|hi", std::regex::icase),
            "greeting",
            [](const std::string&) {
                return "你好！我是 NuClaw AI。\n"
                       "现在支持上下文理解，试试连续提问！";
            }
        });
        
        rules_.push_back({
            std::regex("时间|几点|现在", std::regex::icase),
            "time_query",
            [](const std::string&) {
                auto now = std::chrono::system_clock::now();
                std::time_t t = std::chrono::system_clock::to_time_t(now);
                char buf[100];
                std::strftime(buf, sizeof(buf), "现在时间是 %Y-%m-%d %H:%M:%S", std::localtime(&t));
                return std::string(buf);
            }
        });
        
        rules_.push_back({
            std::regex("(.*)天气|(.*)温度", std::regex::icase),
            "weather_query",
            [this](const std::string& input) {
                return get_weather_for_city(input);
            }
        });
        
        rules_.push_back({
            std::regex("谢谢|感谢|thank", std::regex::icase),
            "thanks",
            [](const std::string&) {
                return "不客气！我会记住我们的对话。";
            }
        });
    }
    
    std::string get_weather_for_city(const std::string& input) {
        if (input.find("北京") != std::string::npos) {
            return "🌤️ 北京今天晴天，气温 25°C，空气质量良好。";
        }
        if (input.find("上海") != std::string::npos) {
            return "☁️ 上海今天多云，气温 22°C，有微风。";
        }
        if (input.find("深圳") != std::string::npos) {
            return "🌧️ 深圳今天小雨，气温 28°C，湿度较高。";
        }
        if (input.find("广州") != std::string::npos) {
            return "⛅ 广州今天阴天，气温 26°C。";
        }
        return "请告诉我具体城市名（如：北京、上海、深圳、广州）";
    }
    
    bool is_contextual_question(const std::string& input, const ChatContext& ctx) {
        bool has_pronoun = std::regex_search(input, 
            std::regex("那|它|这个|那里", std::regex::icase));
        
        if (has_pronoun && ctx.is_valid()) {
            return true;
        }
        return false;
    }
    
    std::string handle_contextual(const std::string& input, ChatContext& ctx) {
        if (ctx.last_intent == "weather_query") {
            std::string reply = get_weather_for_city(input);
            
            if (reply.find("请告诉") == std::string::npos) {
                reply += "\n💡 (我理解了你想比较 " + ctx.last_topic + " 和 " + 
                        extract_topic(input, "weather_query") + " 的天气)";
            }
            
            ctx.last_topic = extract_topic(input, "weather_query");
            return reply;
        }
        
        return "能再说清楚一点吗？或者重新开始一个新话题。";
    }
    
    std::string extract_topic(const std::string& input, const std::string& intent) {
        if (intent == "weather_query") {
            if (input.find("北京") != std::string::npos) return "北京";
            if (input.find("上海") != std::string::npos) return "上海";
            if (input.find("深圳") != std::string::npos) return "深圳";
            if (input.find("广州") != std::string::npos) return "广州";
        }
        return "";
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
                // 解析 JSON body
                json::value jv = json::parse(req.body);
                std::string message = std::string(jv.as_object()["message"].as_string());
                
                // 获取或创建会话
                std::string session_id;
                if (jv.as_object().contains("session_id")) {
                    session_id = std::string(jv.as_object()["session_id"].as_string());
                } else {
                    session_id = session_manager_.generate_session_id();
                }
                
                auto ctx = session_manager_.get_or_create_session(session_id);
                std::string reply = ai_.process(message, *ctx);
                
                // 构建响应
                json::object result;
                result["session_id"] = session_id;
                result["reply"] = reply;
                result["message_count"] = ctx->message_count;
                result["has_context"] = !ctx->last_intent.empty();
                
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
            info["step"] = 4;
            info["title"] = "HTTP Session Management";
            info["description"] = "使用 Session ID 解决 HTTP 无状态问题";
            info["endpoints"] = json::array({
                json::object({{"path", "/chat"}, {"method", "POST"}, 
                             {"params", "message, session_id(optional)"}}),
                json::object({{"path", "/health"}, {"method", "GET"}})
            });
            info["improvements"] = json::array({
                "Session ID 标识会话",
                "服务端维护上下文",
                "支持上下文关联提问"
            });
            resp.body = json::serialize(info);
            return resp;
        }
        
        HttpResponse handle_health() {
            HttpResponse resp;
            json::object health;
            health["status"] = "healthy";
            health["step"] = 4;
            health["feature"] = "http_session";
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
        std::cout << "  NuClaw Step 4 - HTTP Session" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << std::endl;
        
        ChatServer server(io, 8080);
        
        std::cout << "Server listening on http://localhost:8080" << std::endl;
        std::cout << std::endl;
        
        std::cout << "=== 关键改进 ===" << std::endl;
        std::cout << "Step 3 (HTTP): 每次请求独立，上下文丢失" << std::endl;
        std::cout << "Step 4 (Session): Session ID 关联，上下文持久" << std::endl;
        std::cout << std::endl;
        
        std::cout << "=== 测试方式 ===" << std::endl;
        std::cout << "1. 首次对话（获取 session_id）:" << std::endl;
        std::cout << "   curl -X POST http://localhost:8080/chat \\" << std::endl;
        std::cout << "        -H 'Content-Type: application/json' \\" << std::endl;
        std::cout << "        -d '{\"message\":\"北京天气\"}'" << std::endl;
        std::cout << std::endl;
        std::cout << "2. 上下文关联（使用返回的 session_id）:" << std::endl;
        std::cout << "   curl -X POST http://localhost:8080/chat \\" << std::endl;
        std::cout << "        -H 'Content-Type: application/json' \\" << std::endl;
        std::cout << "        -d '{\"session_id\":\"xxx\",\"message\":\"那上海呢\"}'" << std::endl;
        std::cout << std::endl;
        
        std::cout << "=== 对比演示 ===" << std::endl;
        std::cout << "Step 3:" << std::endl;
        std::cout << "  POST /chat {\"message\":\"北京天气\"} → 返回结果" << std::endl;
        std::cout << "  POST /chat {\"message\":\"那上海呢\"} → 不知道上文 ❌" << std::endl;
        std::cout << std::endl;
        std::cout << "Step 4:" << std::endl;
        std::cout << "  POST /chat {\"message\":\"北京天气\"} → 返回 session_id + 结果" << std::endl;
        std::cout << "  POST /chat {\"session_id\":\"xxx\",\"message\":\"那上海呢\"} → ✓ 理解上下文" << std::endl;
        std::cout << std::endl;
        
        std::vector<std::thread> threads;
        auto thread_count = std::thread::hardware_concurrency();
        
        for (unsigned i = 0; i < thread_count; ++i) {
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
