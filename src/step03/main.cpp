// ============================================================================
// Step 3: 规则 AI - 第一个"智能"程序
// ============================================================================
// 演进说明：
//   基于 Step 2 的 HTTP 服务器，实现简单的"智能"回复
//   核心改进：
//     1. 新增 ChatEngine 类，用规则匹配实现"理解"
//     2. 支持简单的上下文（最近一轮对话）
//     3. 暴露 HTTP 问题：每次请求独立，上下文容易丢失
//   编译: g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
//   运行: ./server
//   测试: curl -X POST -d "你好" http://localhost:8080/chat
// ============================================================================

#include <boost/asio.hpp>
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

using boost::asio::ip::tcp;
namespace asio = boost::asio;

// HTTP 请求结构体
struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
};

// HTTP 请求解析
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
    
    // 读取 body
    if (req.headers.count("Content-Length")) {
        int content_length = std::stoi(req.headers["Content-Length"]);
        if (content_length > 0 && content_length < 4096) {
            req.body.resize(content_length);
            stream.read(&req.body[0], content_length);
        }
    }
    
    return req;
}

// 会话上下文（Step 3 新增）
struct ChatContext {
    std::string last_intent;
    std::string last_topic;
    std::chrono::steady_clock::time_point last_time;
};

// ChatEngine - 规则 AI（Step 3 核心新增）
class ChatEngine {
public:
    ChatEngine() { init_rules(); }
    
    std::string process(const std::string& input, ChatContext& ctx) {
        // 检查上下文相关问题
        if (is_contextual_question(input, ctx)) {
            return handle_contextual(input, ctx);
        }
        
        // 规则匹配
        for (const auto& rule : rules_) {
            if (std::regex_search(input, rule.pattern)) {
                ctx.last_intent = rule.intent;
                ctx.last_topic = extract_topic(input, rule.intent);
                ctx.last_time = std::chrono::steady_clock::now();
                return rule.response(input);
            }
        }
        
        return "我不理解。试试：你好、时间、北京天气";
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
                return "你好！我是 NuClaw AI，可以帮你查天气、时间等。";
            }
        });
        
        rules_.push_back({
            std::regex("时间|几点|现在", std::regex::icase),
            "time_query",
            [](const std::string&) {
                auto now = std::chrono::system_clock::now();
                std::time_t t = std::chrono::system_clock::to_time_t(now);
                char buf[100];
                std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
                return std::string(buf);
            }
        });
        
        rules_.push_back({
            std::regex("(.*)天气|(.*)温度", std::regex::icase),
            "weather_query",
            [](const std::string& input) {
                if (input.find("北京") != std::string::npos) 
                    return "北京今天晴天，25°C";
                if (input.find("上海") != std::string::npos) 
                    return "上海今天多云，22°C";
                return "请告诉我城市名（如：北京、上海）";
            }
        });
    }
    
    bool is_contextual_question(const std::string& input, const ChatContext& ctx) {
        // HTTP 问题：每次请求 ctx 都会重置！
        if (std::regex_search(input, std::regex("那|它", std::regex::icase))) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - ctx.last_time).count();
            if (elapsed > 30 || ctx.last_intent.empty()) {
                return false;  // 上下文已过期（HTTP 的问题！）
            }
            return true;
        }
        return false;
    }
    
    std::string handle_contextual(const std::string& input, ChatContext& ctx) {
        if (ctx.last_intent == "weather_query") {
            if (input.find("上海") != std::string::npos) {
                return "上海今天多云，22°C。";
            }
        }
        return "能再说清楚吗？";
    }
    
    std::string extract_topic(const std::string& input, const std::string& intent) {
        if (intent == "weather_query") {
            if (input.find("北京") != std::string::npos) return "北京";
            if (input.find("上海") != std::string::npos) return "上海";
        }
        return "";
    }
};

// Router 类
class Router {
public:
    using Handler = std::function<std::string(const HttpRequest&)>;
    void add_route(const std::string& path, Handler handler) { routes_[path] = handler; }
    std::string handle(const HttpRequest& req) const {
        auto it = routes_.find(req.path);
        return it != routes_.end() ? it->second(req) : 
               R"({"error": "not found"})";
    }
private:
    std::map<std::string, Handler> routes_;
};

// Session 类
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, const Router& router)
        : socket_(std::move(socket)), router_(router) {}
    
    void start() { do_read(); }

private:
    void do_read() {
        auto self = shared_from_this();
        socket_.async_read_some(asio::buffer(buffer_, sizeof(buffer_)),
            [this, self](boost::system::error_code ec, std::size_t len) {
                if (!ec) {
                    HttpRequest req = parse_http_request(buffer_, len);
                    auto response = make_response(router_.handle(req));
                    do_write(response);
                }
            });
    }
    
    void do_write(const std::string& response) {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(response),
            [this, self](boost::system::error_code, std::size_t) {});
    }
    
    std::string make_response(const std::string& body) {
        return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
               "Content-Length: " + std::to_string(body.length()) + "\r\n"
               "Connection: close\r\n\r\n" + body;
    }
    
    tcp::socket socket_;
    char buffer_[4096] = {};
    const Router& router_;
};

// Server 类
class Server {
public:
    Server(asio::io_context& io, short port, const Router& router)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)), router_(router) { do_accept(); }

private:
    void do_accept() {
        acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<Session>(std::move(socket), router_)->start();
            }
            do_accept();
        });
    }
    
    tcp::acceptor acceptor_;
    const Router& router_;
};

int main() {
    try {
        asio::io_context io;
        
        std::cout << "========================================" << std::endl;
        std::cout << "  NuClaw Step 3 - Rule-based AI (HTTP)" << std::endl;
        std::cout << "========================================" << std::endl;
        
        ChatEngine ai;
        Router router;
        
        router.add_route("/", [](const HttpRequest&) {
            return R"({"status": "ok", "step": 3, "message": "Rule-based AI"})";
        });
        
        router.add_route("/chat", [&ai](const HttpRequest& req) {
            ChatContext ctx;  // 注意：每次请求新的 ctx，HTTP 问题！
            std::string input = req.body.empty() ? "你好" : req.body;
            std::string reply = ai.process(input, ctx);
            return R"({"input": ")" + input + R"(", "reply": ")" + reply + "\"})";
        });
        
        Server server(io, 8080, router);
        
        std::cout << "Server: http://localhost:8080" << std::endl;
        std::cout << std::endl;
        std::cout << "Test: curl -X POST -d \"你好\" http://localhost:8080/chat" << std::endl;
        std::cout << std::endl;
        std::cout << "[!] Known issue: HTTP is stateless." << std::endl;
        std::cout << "    Context lost between requests." << std::endl;
        std::cout << "    Next: WebSocket to fix this." << std::endl;
        std::cout << std::endl;
        
        std::vector<std::thread> threads;
        for (unsigned i = 0; i < std::thread::hardware_concurrency(); ++i) {
            threads.emplace_back([&io]() { io.run(); });
        }
        for (auto& t : threads) t.join();
        
    } catch (std::exception& e) {
        std::cerr << "[!] Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
