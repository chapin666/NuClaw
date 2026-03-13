// ============================================================================
// Step 4: WebSocket 实时通信 - 解决 HTTP 上下文问题
// ============================================================================
// 演进说明：
//   基于 Step 3 的规则 AI，解决 HTTP 无状态问题
//   Step 3 的问题：
//     1. 每次请求都是新的 HTTP 连接
//     2. 上下文（ChatContext）无法持久化
//     3. 用户问"北京天气"后再问"那上海呢"，AI 无法理解
//   本章解决：
//     1. 用 WebSocket 建立长连接
//     2. 在连接期间保持上下文
//     3. 支持服务器主动推送消息
// 编译: g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
// 运行: ./server
// HTTP测试: curl http://localhost:8080
// WebSocket测试: wscat -c ws://localhost:8080/ws
//              然后输入: 你好
//                        北京天气
//                        那上海呢  (AI 会理解是问天气)
// ============================================================================

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
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
#include <time>

using boost::asio::ip::tcp;
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;

// ============================================================================
// HTTP 请求结构体（从 Step 3 继承）
// ============================================================================
struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
    
    // 新增：检测 WebSocket 升级请求
    bool is_websocket_upgrade() const {
        auto it = headers.find("Upgrade");
        if (it != headers.end() && it->second == "websocket") {
            // 还要检查 Connection: Upgrade
            auto conn = headers.find("Connection");
            return conn != headers.end() && 
                   conn->second.find("Upgrade") != std::string::npos;
        }
        return false;
    }
};

// ============================================================================
// HTTP 请求解析（从 Step 3 继承）
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
    
    return req;
}

// ============================================================================
// 会话上下文（从 Step 3 继承，但现在是持久的！）
// ============================================================================
// 关键改进：WebSocket 长连接期间，这个 context 一直存在
// ============================================================================
struct ChatContext {
    std::string last_intent;
    std::string last_topic;
    std::chrono::steady_clock::time_point last_time;
    int message_count = 0;  // 新增：记录对话轮数
    
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
// ChatEngine（从 Step 3 继承，但现在能真正利用上下文了）
// ============================================================================
class ChatEngine {
public:
    ChatEngine() {
        init_rules();
    }
    
    // 关键改进：ctx 现在是持久的，可以真正记住上下文
    std::string process(const std::string& input, ChatContext& ctx) {
        ctx.message_count++;
        
        // 检查是否是上下文相关的提问
        if (is_contextual_question(input, ctx)) {
            std::string reply = handle_contextual(input, ctx);
            ctx.last_time = std::chrono::steady_clock::now();
            return reply;
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
        
        return "我不理解你的问题，可以试试：你好、时间、北京天气、那上海呢";
    }
    
    // 新增：主动推送消息（服务器可以主动发消息给客户端）
    std::string get_welcome_message() {
        return "👋 欢迎来到 NuClaw AI！\n"
               "我已经记住你的对话，可以问上下文相关的问题。\n"
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
                return "不客气！我们已对话了多轮，我会记住上下文。";
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
    
    // 关键改进：真正能工作的上下文检测
    bool is_contextual_question(const std::string& input, const ChatContext& ctx) {
        // 检查是否有代词指代
        bool has_pronoun = std::regex_search(input, 
            std::regex("那|它|这个|那里", std::regex::icase));
        
        if (has_pronoun && ctx.is_valid()) {
            return true;
        }
        return false;
    }
    
    std::string handle_contextual(const std::string& input, ChatContext& ctx) {
        // 根据上次意图处理上下文
        if (ctx.last_intent == "weather_query") {
            // 用户之前问天气，现在说"那上海呢"
            std::string reply = get_weather_for_city(input);
            
            // 添加上下文理解的说明
            if (reply.find("请告诉") == std::string::npos) {
                reply += "\n💡 (我理解了你想比较 " + ctx.last_topic + " 和 " + 
                        extract_topic(input, "weather_query") + " 的天气)";
            }
            
            // 更新上下文
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
// 新增：WebSocket Session 类 - 核心改进！
// ============================================================================
// WebSocket 连接期间：
// - 长连接保持（不关闭）
// - 上下文持久化（ChatContext 一直在内存中）
// - 服务器可以主动推送消息
// ============================================================================
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    WebSocketSession(tcp::socket socket, ChatEngine& ai)
        : ws_(std::move(socket)), ai_(ai) {}
    
    void start() {
        // WebSocket 握手
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
        
        // 主动推送欢迎消息！这是 HTTP 做不到的
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
            std::cout << "[-] WebSocket closed (对话 " << ctx_.message_count 
                      << " 轮)" << std::endl;
            return;
        }
        if (ec) {
            std::cerr << "[!] WebSocket read error: " << ec.message() << std::endl;
            return;
        }
        
        std::string msg = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());
        
        std::cout << "[<] User: " << msg << std::endl;
        
        // 关键：使用持久的 ChatContext！
        std::string reply = ai_.process(msg, ctx_);
        
        std::cout << "[>] AI: " << reply.substr(0, 50) << "..." << std::endl;
        
        ws_.text(true);
        ws_.async_write(asio::buffer(reply),
            beast::bind_front_handler(&WebSocketSession::on_write, shared_from_this()));
    }
    
    void on_write(beast::error_code ec, std::size_t bytes) {
        if (ec) {
            std::cerr << "[!] WebSocket write error: " << ec.message() << std::endl;
            return;
        }
        // 继续读取下一条消息
        do_read();
    }
    
    websocket::stream<tcp::socket> ws_;
    ChatEngine& ai_;
    beast::flat_buffer buffer_;
    
    // 关键：WebSocket 连接期间，这个 context 一直存在！
    ChatContext ctx_;
};

// ============================================================================
// Router 类（从 Step 3 继承）
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
// Session 类（从 Step 3 继承，但增加了 WebSocket 升级处理）
// ============================================================================
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, const Router& router, ChatEngine& ai)
        : socket_(std::move(socket)), router_(router), ai_(ai) {}
    
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
                    
                    // 关键改进：检测 WebSocket 升级
                    if (req.is_websocket_upgrade()) {
                        std::cout << "[i] Upgrading HTTP to WebSocket..." << std::endl;
                        
                        // 将 socket 移交给 WebSocketSession
                        std::make_shared<WebSocketSession>(
                            std::move(socket_), ai_)->start();
                        
                        // 注意：当前 Session 对象会析构，
                        // 但 WebSocketSession 会继续处理连接
                    } else {
                        // 普通 HTTP 请求
                        auto response = make_response(router_.handle(req));
                        do_write(response);
                    }
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
    ChatEngine& ai_;
};

// ============================================================================
// Server 类（从 Step 3 继承）
// ============================================================================
class Server {
public:
    Server(asio::io_context& io, short port, const Router& router, ChatEngine& ai)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)), 
          router_(router), ai_(ai) {
        std::cout << "[i] Server binding to port " << port << std::endl;
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(
                        std::move(socket), router_, ai_)->start();
                }
                do_accept();
            }
        );
    }
    
    tcp::acceptor acceptor_;
    const Router& router_;
    ChatEngine& ai_;
};

// ============================================================================
// 主函数
// ============================================================================
int main() {
    try {
        asio::io_context io;
        
        std::cout << "========================================" << std::endl;
        std::cout << "  NuClaw Step 4 - WebSocket Real-time" << std::endl;
        std::cout << "========================================" << std::endl;
        
        ChatEngine ai;
        Router router;
        
        // HTTP 路由（向后兼容）
        router.add_route("/", [](const HttpRequest& req) {
            return R"({
    "status": "ok",
    "step": 4,
    "message": "WebSocket AI Server",
    "endpoints": {
        "/": "This help message (HTTP)",
        "/ws": "WebSocket endpoint for real-time chat"
    },
    "improvements": [
        "WebSocket keeps connection open",
        "Context persists during session",
        "Server can push messages proactively"
    ]
})";
        });
        
        // 健康检查
        router.add_route("/health", [](const HttpRequest& req) {
            return R"({"status": "healthy", "step": 4, "feature": "websocket"})";
        });
        
        Server server(io, 8080, router, ai);
        
        std::cout << "Server listening on http://localhost:8080" << std::endl;
        std::cout << "WebSocket endpoint: ws://localhost:8080/ws" << std::endl;
        std::cout << std::endl;
        
        std::cout << "=== 关键改进 ===" << std::endl;
        std::cout << "Step 3 (HTTP): 每次请求独立，上下文丢失" << std::endl;
        std::cout << "Step 4 (WebSocket): 长连接保持，上下文持久" << std::endl;
        std::cout << std::endl;
        
        std::cout << "=== 测试方式 ===" << std::endl;
        std::cout << "1. 安装 wscat: npm install -g wscat" << std::endl;
        std::cout << "2. 连接: wscat -c ws://localhost:8080/ws" << std::endl;
        std::cout << "3. 输入消息:" << std::endl;
        std::cout << "   > 你好" << std::endl;
        std::cout << "   > 北京天气" << std::endl;
        std::cout << "   > 那上海呢    (✓ AI 会理解是问天气)" << std::endl;
        std::cout << std::endl;
        
        std::cout << "=== 对比演示 ===" << std::endl;
        std::cout << "HTTP 版 (Step 3):" << std::endl;
        std::cout << "  请求1: 北京天气 → 返回结果，连接关闭" << std::endl;
        std::cout << "  请求2: 那上海呢 → 不知道上文，无法理解" << std::endl;
        std::cout << std::endl;
        std::cout << "WebSocket 版 (Step 4):" << std::endl;
        std::cout << "  连接建立 → 保持打开" << std::endl;
        std::cout << "  消息1: 北京天气 → 返回结果，上下文保留" << std::endl;
        std::cout << "  消息2: 那上海呢 → ✓ 理解上下文，返回上海天气" << std::endl;
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
