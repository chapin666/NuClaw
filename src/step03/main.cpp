// ============================================================================
// Step 3: 第一个"AI" - 基于 HTTP 的规则回复
// ============================================================================
// 演进说明：
//   基于 Step 2 的 HTTP 服务器，实现简单的"智能"回复
//   1. 新增 ChatEngine 类处理消息逻辑
//   2. 规则匹配：关键词 → 回复模板
//   3. 简单的上下文记忆（最近一轮对话）
//   4. 暴露 HTTP 问题：每次请求独立，无持久连接
// 编译: g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
// 运行: ./server
// 测试: curl -X POST -d "你好" http://localhost:8080/chat
//       curl -X POST -d "时间" http://localhost:8080/chat
//       curl -X POST -d "北京天气" http://localhost:8080/chat
// 问题: 每次请求都是新的 HTTP 连接，上下文容易丢失
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

// ============================================================================
// HTTP 请求结构体（从 Step 2 继承）
// ============================================================================
struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
};

// ============================================================================
// HTTP 请求解析（从 Step 2 继承）
// ============================================================================
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
    
    // 读取 body（对于 POST 请求）
    if (req.headers.count("Content-Length")) {
        int content_length = std::stoi(req.headers["Content-Length"]);
        if (content_length > 0 && content_length < 4096) {
            req.body.resize(content_length);
            stream.read(&req.body[0], content_length);
        }
    }
    
    return req;
}

// ============================================================================
// 新增：会话上下文
// ============================================================================
// 记录最近一轮对话，用于理解上下文
// 问题：HTTP 是无状态的，每次请求都是新的连接
//       这个 context 只在单次请求内有效，请求结束后就销毁了
// ============================================================================
struct ChatContext {
    std::string last_intent;      // 上次的意图
    std::string last_topic;       // 上次的话题
    std::chrono::steady_clock::time_point last_time;
};

// ============================================================================
// 新增：ChatEngine - 简单的规则 AI
// ============================================================================
// 用规则匹配实现"智能"回复
// 虽然简单，但能让程序看起来有理解能力
// ============================================================================
class ChatEngine {
public:
    ChatEngine() {
        // 初始化规则库
        init_rules();
    }
    
    std::string process(const std::string& input, ChatContext& ctx) {
        // 检查是否是上下文相关的提问（如"那上海呢？"）
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
        
        return "我不理解你的问题，可以试试：你好、时间、北京天气";
    }

private:
    struct Rule {
        std::regex pattern;
        std::string intent;
        std::function<std::string(const std::string&)> response;
    };
    
    std::vector<Rule> rules_;
    
    void init_rules() {
        // 问候规则
        rules_.push_back({
            std::regex("你好|嗨|hello|hi", std::regex::icase),
            "greeting",
            [](const std::string&) {
                return "你好！很高兴见到你。我是 NuClaw AI，可以帮你查天气、时间等。";
            }
        });
        
        // 时间查询
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
        
        // 天气查询
        rules_.push_back({
            std::regex("(.*)天气|(.*)温度", std::regex::icase),
            "weather_query",
            [](const std::string& input) {
                // 简单提取城市名（实际应该用 NLP）
                if (input.find("北京") != std::string::npos) {
                    return "北京今天晴天，气温 25°C，空气质量良好。";
                }
                if (input.find("上海") != std::string::npos) {
                    return "上海今天多云，气温 22°C，有微风。";
                }
                if (input.find("深圳") != std::string::npos) {
                    return "深圳今天小雨，气温 28°C，湿度较高。";
                }
                return "请告诉我具体城市名（如：北京、上海、深圳）";
            }
        });
        
        // 感谢
        rules_.push_back({
            std::regex("谢谢|感谢|thank", std::regex::icase),
            "thanks",
            [](const std::string&) {
                return "不客气！有问题随时问我。";
            }
        });
    }
    
    bool is_contextual_question(const std::string& input, const ChatContext& ctx) {
        // 检查是否是代词指代（如"那...呢"、"它..."）
        // 但 HTTP 是无状态的，ctx 每次都会重置！
        // 这是演示 HTTP 局限性的关键
        
        if (std::regex_search(input, std::regex("那|它|这个", std::regex::icase))) {
            // 检查上次对话是否在 30 秒内
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - ctx.last_time).count();
            
            if (elapsed > 30 || ctx.last_intent.empty()) {
                return false;  // 上下文已过期
            }
            return true;
        }
        return false;
    }
    
    std::string handle_contextual(const std::string& input, const ChatContext& ctx) {
        // 尝试用上下文理解
        if (ctx.last_intent == "weather_query") {
            // 用户之前问天气，现在说"那上海呢"
            if (input.find("上海") != std::string::npos) {
                return "上海今天多云，气温 22°C，有微风。";
            }
            return "你想了解哪个城市的天气？";
        }
        return "能再说清楚一点吗？";
    }
    
    std::string extract_topic(const std::string& input, const std::string& intent) {
        if (intent == "weather_query") {
            if (input.find("北京") != std::string::npos) return "北京";
            if (input.find("上海") != std::string::npos) return "上海";
            if (input.find("深圳") != std::string::npos) return "深圳";
        }
        return "";
    }
};

// ============================================================================
// Router 类（从 Step 2 继承）
// ============================================================================
class Router {
public:
    using Handler = std::function<std::string(const HttpRequest&)>;
    
    void add_route(const std::string& path, Handler handler) {
        routes_[path] = handler;
    }
    
    std::string handle(const HttpRequest& req) const {
        auto it = routes_.find(req.path);
        if (it != routes_.end()) {
            return it->second(req);
        }
        return R"({"error": "not found", "path": ")" + req.path + "\"}";
    }

private:
    std::map<std::string, Handler> routes_;
};

// ============================================================================
// Session 类（从 Step 2 继承）
// ============================================================================
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, const Router& router)
        : socket_(std::move(socket)), router_(router) {}
    
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
                    std::cout << "[+] " << req.method << " " << req.path << std::endl;
                    
                    auto response = make_response(router_.handle(req));
                    do_write(response);
                }
            }
        );
    }
    
    void do_write(const std::string& response) {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(response),
            [this, self](boost::system::error_code, std::size_t) {}
        );
    }
    
    std::string make_response(const std::string& body) {
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: application/json; charset=utf-8\r\n"
               "Content-Length: " + std::to_string(body.length()) + "\r\n"
               "Connection: close\r\n"
               "\r\n" + body;
    }
    
    tcp::socket socket_;
    char buffer_[4096] = {};
    const Router& router_;
};

// ============================================================================
// Server 类（从 Step 2 继承）
// ============================================================================
class Server {
public:
    Server(asio::io_context& io, short port, const Router& router)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)), router_(router) {
        std::cout << "[i] Server binding to port " << port << std::endl;
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket), router_)->start();
                }
                do_accept();
            }
        );
    }
    
    tcp::acceptor acceptor_;
    const Router& router_;
};

// ============================================================================
// 主函数
// ============================================================================
int main() {
    try {
        asio::io_context io;
        
        std::cout << "========================================" << std::endl;
        std::cout << "  NuClaw Step 3 - Rule-based AI (HTTP)" << std::endl;
        std::cout << "========================================" << std::endl;
        
        // 创建 AI 引擎
        ChatEngine ai;
        
        Router router;
        
        // 主页路由
        router.add_route("/", [](const HttpRequest& req) {
            return R"({
    "status": "ok",
    "step": 3,
    "message": "Rule-based AI Server",
    "endpoints": {
        "/": "This help message",
        "/chat": "POST message to chat with AI"
    },
    "note": "HTTP is stateless - context may be lost between requests"
})";
        });
        
        // AI 对话路由 - 核心功能
        router.add_route("/chat", [&ai](const HttpRequest& req) {
            // 注意：每次请求都是新的 ChatContext！
            // 这就是 HTTP 的问题：上下文无法持久化
            ChatContext ctx;
            
            std::string user_input = req.body.empty() ? "你好" : req.body;
            std::string reply = ai.process(user_input, ctx);
            
            return R"({"input": ")" + user_input + R"(", "reply": ")" + reply + R"("})";
        });
        
        // 健康检查
        router.add_route("/health", [](const HttpRequest& req) {
            return R"({"status": "healthy", "step": 3})";
        });
        
        Server server(io, 8080, router);
        
        std::cout << "Server listening on http://localhost:8080" << std::endl;
        std::cout << std::endl;
        std::cout << "Test commands:" << std::endl;
        std::cout << "  curl http://localhost:8080/" << std::endl;
        std::cout << "  curl -X POST -d \"你好\" http://localhost:8080/chat" << std::endl;
        std::cout << "  curl -X POST -d \"北京天气\" http://localhost:8080/chat" << std::endl;
        std::cout << "  curl -X POST -d \"时间\" http://localhost:8080/chat" << std::endl;
        std::cout << std::endl;
        std::cout << "[!] Known issue: HTTP is stateless." << std::endl;
        std::cout << "    Try: Ask '北京天气' then '那上海呢' - it may not understand." << std::endl;
        std::cout << "    This will be fixed in Step 4 with WebSocket." << std::endl;
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
