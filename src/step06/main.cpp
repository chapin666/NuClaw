// ============================================================================
// Step 6: 工具调用（硬编码版）- LLM 使用工具获取实时数据
// ============================================================================
// 演进说明：
//   基于 Step 5 的 LLM，添加工具调用能力
//   Step 5 的问题：
//     1. LLM 知识是静态的，不知道实时信息
//     2. 无法获取天气、股价、数据库查询等动态数据
//   本章解决：
//     1. 添加硬编码工具（天气、时间、计算）
//     2. LLM 决定调用哪个工具
//     3. 执行工具，返回结果给 LLM 生成回复
//   本章暴露的问题：
//     每添加一个工具都要改代码，硬编码难以维护
//     下一章解决：Tool 系统架构
// 编译: g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
// 运行: ./server
// 测试: wscat -c ws://localhost:8080/ws
//       试试：北京天气如何？ / 1+1等于几？ / 现在几点？
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
#include <cstdlib>
#include <cmath>

using boost::asio::ip::tcp;
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace json = boost::json;

// ============================================================================
// 新增：工具基类（硬编码版的起点）
// ============================================================================
// 注意：这是最简单的实现，每个工具都是独立函数
// 问题：添加新工具需要改多处代码
// ============================================================================

// 工具执行结果
struct ToolResult {
    bool success;
    std::string data;      // JSON 格式
    std::string error;
};

// ============================================================================
// 硬编码工具实现
// ============================================================================

// 工具1：天气查询（模拟）
ToolResult tool_get_weather(const std::string& city) {
    ToolResult result;
    result.success = true;
    
    // 模拟天气数据
    if (city == "北京" || city == "beijing") {
        result.data = R"({"city": "北京", "weather": "晴天", "temp": 25, "aqi": 50})";
    } else if (city == "上海" || city == "shanghai") {
        result.data = R"({"city": "上海", "weather": "多云", "temp": 22, "aqi": 45})";
    } else if (city == "深圳" || city == "shenzhen") {
        result.data = R"({"city": "深圳", "weather": "小雨", "temp": 28, "humidity": 80})";
    } else if (city == "广州" || city == "guangzhou") {
        result.data = R"({"city": "广州", "weather": "阴天", "temp": 26})";
    } else {
        result.success = false;
        result.error = "不支持的城市: " + city + "，目前支持：北京、上海、深圳、广州";
    }
    
    return result;
}

// 工具2：当前时间
ToolResult tool_get_time(const std::string& timezone = "") {
    ToolResult result;
    result.success = true;
    
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    
    char buf[100];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    
    json::object obj;
    obj["datetime"] = buf;
    obj["timestamp"] = static_cast<int64_t>(t);
    obj["timezone"] = timezone.empty() ? "本地时间" : timezone;
    
    result.data = json::serialize(obj);
    return result;
}

// 工具3：计算器
ToolResult tool_calculate(const std::string& expression) {
    ToolResult result;
    
    try {
        // 超简化版计算器，仅支持基本运算
        // 实际应该用表达式解析库
        double a, b;
        char op;
        std::istringstream iss(expression);
        iss >> a >> op >> b;
        
        double res = 0;
        switch (op) {
            case '+': res = a + b; break;
            case '-': res = a - b; break;
            case '*': res = a * b; break;
            case '/': 
                if (b == 0) {
                    result.success = false;
                    result.error = "除数不能为零";
                    return result;
                }
                res = a / b; 
                break;
            default:
                result.success = false;
                result.error = "不支持的操作符: " + std::string(1, op);
                return result;
        }
        
        result.success = true;
        json::object obj;
        obj["expression"] = expression;
        obj["result"] = res;
        result.data = json::serialize(obj);
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error = "计算错误: " + std::string(e.what());
    }
    
    return result;
}

// ============================================================================
// 新增：工具执行器（硬编码版）
// ============================================================================
// 问题：每加一个工具都要在这里添加 case
// 这就是硬编码的痛苦，下一章用注册表模式解决
// ============================================================================
class ToolExecutor {
public:
    // 解析工具调用请求
    struct ToolCall {
        std::string name;
        std::string arguments;
    };
    
    // 执行工具
    ToolResult execute(const ToolCall& call) {
        std::cout << "[⚙️ Executing tool] " << call.name << "(" << call.arguments << ")" << std::endl;
        
        // 硬编码的工具分发
        // 每添加一个新工具，都要修改这里！
        if (call.name == "get_weather" || call.name == "天气") {
            return tool_get_weather(call.arguments);
        }
        else if (call.name == "get_time" || call.name == "时间") {
            return tool_get_time(call.arguments);
        }
        else if (call.name == "calculate" || call.name == "计算") {
            return tool_calculate(call.arguments);
        }
        
        ToolResult result;
        result.success = false;
        result.error = "未知工具: " + call.name + "\n"
                      "可用工具: get_weather, get_time, calculate";
        return result;
    }
    
    // 获取可用工具列表（用于提示 LLM）
    std::string get_tools_description() const {
        return R"(可用工具：
1. get_weather(city: string) - 查询城市天气
   示例: get_weather("北京")
   
2. get_time(timezone?: string) - 获取当前时间
   示例: get_time()
   
3. calculate(expression: string) - 计算表达式
   示例: calculate("1 + 2")

使用方式：
如果用户问题需要实时数据，请回复：TOOL_CALL: {"tool": "工具名", "args": "参数"}
否则直接回答。)";
    }
};

// ============================================================================
// 新增：LLM Client（支持工具调用）
// ============================================================================
class LLMClient {
public:
    LLMClient(const std::string& api_key) : api_key_(api_key) {}
    
    // 判断是否需要调用工具
    bool needs_tool(const std::string& user_input) const {
        // 简化版：关键词匹配
        // 真实 LLM 会基于语义判断
        std::vector<std::string> tool_keywords = {
            "天气", "温度", "下雨", "晴天",
            "时间", "几点", "日期",
            "计算", "等于", "+", "-", "*", "/", "加", "减", "乘", "除"
        };
        
        for (const auto& kw : tool_keywords) {
            if (user_input.find(kw) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
    
    // 解析工具调用（简化版）
    ToolExecutor::ToolCall parse_tool_call(const std::string& user_input) {
        ToolExecutor::ToolCall call;
        
        // 超简化解析，实际应该用 NLP 或让 LLM 生成结构化输出
        if (user_input.find("天气") != std::string::npos ||
            user_input.find("温度") != std::string::npos) {
            call.name = "get_weather";
            // 提取城市名（简化）
            if (user_input.find("北京") != std::string::npos) call.arguments = "北京";
            else if (user_input.find("上海") != std::string::npos) call.arguments = "上海";
            else if (user_input.find("深圳") != std::string::npos) call.arguments = "深圳";
            else if (user_input.find("广州") != std::string::npos) call.arguments = "广州";
            else call.arguments = "北京"; // 默认
        }
        else if (user_input.find("时间") != std::string::npos ||
                 user_input.find("几点") != std::string::npos) {
            call.name = "get_time";
            call.arguments = "";
        }
        else if (user_input.find("计算") != std::string::npos ||
                 user_input.find("等于") != std::string::npos ||
                 user_input.find("+") != std::string::npos) {
            call.name = "calculate";
            // 尝试提取表达式（简化）
            call.arguments = extract_expression(user_input);
        }
        
        return call;
    }
    
    // 生成最终回复（模拟 LLM 根据工具结果生成自然语言）
    std::string generate_response(const std::string& user_input, 
                                   const ToolResult& tool_result,
                                   const ToolExecutor::ToolCall& call) {
        if (!tool_result.success) {
            return "抱歉，工具调用失败: " + tool_result.error;
        }
        
        // 解析工具返回的 JSON
        try {
            json::value data = json::parse(tool_result.data);
            
            if (call.name == "get_weather") {
                std::string city = data.as_object()["city"].as_string();
                std::string weather = data.as_object()["weather"].as_string();
                int temp = static_cast<int>(data.as_object()["temp"].as_int64());
                
                return city + "今天" + weather + "，气温" + std::to_string(temp) + "°C。";
            }
            else if (call.name == "get_time") {
                std::string dt = data.as_object()["datetime"].as_string();
                return "当前时间是 " + dt;
            }
            else if (call.name == "calculate") {
                std::string expr = data.as_object()["expression"].as_string();
                double res = data.as_object()["result"].as_double();
                
                std::ostringstream oss;
                oss << expr << " = " << res;
                return oss.str();
            }
        } catch (const std::exception& e) {
            return "工具执行成功，但解析结果出错: " + std::string(e.what());
        }
        
        return "工具执行成功";
    }
    
    // 直接回答（不需要工具）
    std::string direct_reply(const std::string& user_input) {
        // 模拟 LLM 对话能力
        if (user_input.find("你好") != std::string::npos ||
            user_input.find("hello") != std::string::npos) {
            return "你好！我是带工具调用能力的 AI。\n"
                   "我可以帮你：查天气、算数学、看时间。\n"
                   "试试问我：北京天气如何？";
        }
        
        if (user_input.find("工具") != std::string::npos ||
            user_input.find("能做什么") != std::string::npos) {
            return "我可以使用以下工具：\n"
                   "🌤️ get_weather(city) - 查询城市天气\n"
                   "🕐 get_time() - 获取当前时间\n"
                   "🧮 calculate(expr) - 计算数学表达式\n\n"
                   "试试：北京天气 / 现在几点 / 25 * 4";
        }
        
        return "我可以帮你查天气、算数学、看时间。\n"
               "或者你可以问我'你能做什么'了解更多。";
    }

private:
    std::string api_key_;
    
    std::string extract_expression(const std::string& input) {
        // 超简化：直接返回输入，实际应该解析数学表达式
        // 移除中文，保留数字和运算符
        std::string expr;
        for (char c : input) {
            if (std::isdigit(c) || c == '+' || c == '-' || c == '*' || c == '/' || c == '.' || c == ' ') {
                expr += c;
            }
        }
        // 替换中文运算符
        size_t pos;
        while ((pos = expr.find("加")) != std::string::npos) expr.replace(pos, 3, "+");
        while ((pos = expr.find("减")) != std::string::npos) expr.replace(pos, 3, "-");
        while ((pos = expr.find("乘")) != std::string::npos) expr.replace(pos, 3, "*");
        while ((pos = expr.find("除")) != std::string::npos) expr.replace(pos, 3, "/");
        while ((pos = expr.find("等于")) != std::string::npos) expr.replace(pos, 6, "");
        
        return expr.empty() ? "1 + 1" : expr;
    }
};

// ============================================================================
// ChatEngine（Step 5 的扩展版，加入工具调用）
// ============================================================================
class ChatEngine {
public:
    ChatEngine() : llm_(get_api_key()) {}
    
    std::string process(const std::string& user_input) {
        std::cout << "[🧠 Processing] \"" << user_input << "\"" << std::endl;
        
        // 第一步：判断是否需要工具
        if (llm_.needs_tool(user_input)) {
            std::cout << "  → Needs tool" << std::endl;
            
            // 第二步：解析工具调用
            auto tool_call = llm_.parse_tool_call(user_input);
            
            // 第三步：执行工具
            auto result = tool_executor_.execute(tool_call);
            
            // 第四步：LLM 根据结果生成回复
            return llm_.generate_response(user_input, result, tool_call);
        }
        else {
            std::cout << "  → Direct reply" << std::endl;
            return llm_.direct_reply(user_input);
        }
    }
    
    std::string get_welcome_message() {
        return "👋 欢迎来到 NuClaw Step 6！\n\n"
               "🔧 现在支持工具调用了！\n\n"
               "试试：\n"
               "- 北京天气如何？\n"
               "- 现在几点？\n"
               "- 25 * 4 等于多少？\n"
               "- 你能做什么？\n\n"
               "⚠️ 注意：工具是硬编码的，添加新工具需要改代码\n"
               "（下一章会解决这个问题）";
    }

private:
    LLMClient llm_;
    ToolExecutor tool_executor_;
    
    std::string get_api_key() {
        const char* key = std::getenv("OPENAI_API_KEY");
        return key ? key : "";
    }
};

// ============================================================================
// WebSocket Session（从 Step 5 继承，简化版）
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
        if (ec) return;
        
        ws_.text(true);
        ws_.async_write(asio::buffer(ai_.get_welcome_message()),
            beast::bind_front_handler(&WebSocketSession::on_write, shared_from_this()));
    }
    
    void do_read() {
        ws_.async_read(buffer_,
            beast::bind_front_handler(&WebSocketSession::on_read, shared_from_this()));
    }
    
    void on_read(beast::error_code ec, std::size_t) {
        if (ec == websocket::error::closed) return;
        if (ec) return;
        
        std::string msg = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());
        
        std::string reply = ai_.process(msg);
        
        ws_.text(true);
        ws_.async_write(asio::buffer(reply),
            beast::bind_front_handler(&WebSocketSession::on_write, shared_from_this()));
    }
    
    void on_write(beast::error_code ec, std::size_t) {
        if (!ec) do_read();
    }
    
    websocket::stream<tcp::socket> ws_;
    ChatEngine& ai_;
    beast::flat_buffer buffer_;
};

// ============================================================================
// HTTP/Session/Server（简化版）
// ============================================================================
struct HttpRequest {
    std::string method, path;
    std::map<std::string, std::string> headers;
    bool is_websocket_upgrade() const {
        auto it = headers.find("Upgrade");
        return it != headers.end() && it->second == "websocket";
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

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, ChatEngine& ai)
        : socket_(std::move(socket)), ai_(ai) {}
    
    void start() {
        socket_.async_read_some(asio::buffer(buffer_, sizeof(buffer_)),
            [this, self = shared_from_this()](boost::system::error_code ec, std::size_t len) {
                if (!ec) {
                    auto req = parse_http_request(buffer_, len);
                    if (req.is_websocket_upgrade()) {
                        std::make_shared<WebSocketSession>(std::move(socket_), ai_)->start();
                    } else {
                        std::string body = R"({"status": "ok", "step": 6, "feature": "tools"})";
                        std::string response = "HTTP/1.1 200 OK\r\nContent-Length: " + 
                                               std::to_string(body.length()) + "\r\n\r\n" + body;
                        asio::async_write(socket_, asio::buffer(response), 
                            [](boost::system::error_code, std::size_t) {});
                    }
                }
            });
    }

private:
    tcp::socket socket_;
    ChatEngine& ai_;
    char buffer_[4096] = {};
};

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
            });
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
        std::cout << "  NuClaw Step 6 - Tool Calling" << std::endl;
        std::cout << "========================================" << std::endl;
        
        ChatEngine ai;
        Server server(io, 8080, ai);
        
        std::cout << std::endl;
        std::cout << "Server: http://localhost:8080" << std::endl;
        std::cout << "WebSocket: ws://localhost:8080/ws" << std::endl;
        std::cout << std::endl;
        
        std::cout << "=== Agent Loop 完整了！ ===" << std::endl;
        std::cout << "Step 3: 输入 → 规则 → 输出" << std::endl;
        std::cout << "Step 4: + WebSocket 保持连接" << std::endl;
        std::cout << "Step 5: + LLM 理解语义" << std::endl;
        std::cout << "Step 6: + 工具调用（获取实时数据）" << std::endl;
        std::cout << std::endl;
        std::cout << "现在你有一个完整的 Agent：" << std::endl;
        std::cout << "感知 → 理解 → 决策 → 行动 → 响应" << std::endl;
        std::cout << std::endl;
        
        std::cout << "⚠️ 但工具是硬编码的，添加新工具需要改代码" << std::endl;
        std::cout << "下一章：Tool 系统架构（解决扩展性问题）" << std::endl;
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
