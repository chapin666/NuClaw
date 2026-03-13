// src/step08/main.cpp
// Step 8: Tools 系统（下）- 工具生态：HTTP、文件操作、安全沙箱

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <memory>
#include <filesystem>

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
namespace json = boost::json;
using tcp = asio::ip::tcp;

namespace fs = std::filesystem;

// ============ 安全沙箱 ============

class Sandbox {
public:
    // 允许访问的路径白名单
    void allow_path(const fs::path& path) {
        allowed_paths_.push_back(fs::canonical(path));
    }
    
    // 检查路径是否在白名单内
    bool is_allowed(const fs::path& path) const {
        try {
            auto canon = fs::canonical(path);
            for (const auto& allowed : allowed_paths_) {
                // 检查是否是白名单路径的子目录
                auto [it, _] = std::mismatch(
                    allowed.begin(), allowed.end(),
                    canon.begin(), canon.end()
                );
                if (it == allowed.end()) return true;
            }
        } catch (...) {
            return false;
        }
        return false;
    }
    
    // 安全检查：防止路径遍历攻击
    static bool is_safe_path(const std::string& path) {
        // 检查 .. 和绝对路径
        if (path.find("..") != std::string::npos) return false;
        if (!path.empty() && path[0] == '/') return false;  // 禁止绝对路径（简化）
        return true;
    }

private:
    std::vector<fs::path> allowed_paths_;
};

// ============ Tool 基类 ============

class Tool {
public:
    virtual ~Tool() = default;
    virtual std::string get_name() const = 0;
    virtual std::string get_description() const = 0;
    virtual json::value execute(const json::object& args) = 0;
    
    // 带错误处理的包装
    json::value execute_safe(const json::object& args) {
        try {
            return execute(args);
        } catch (const std::exception& e) {
            json::object error;
            error["success"] = false;
            error["error"] = e.what();
            return error;
        }
    }
};

// ============ HTTP 工具 ============

class HttpGetTool : public Tool {
public:
    std::string get_name() const override { return "http_get"; }
    std::string get_description() const override { 
        return "Send HTTP GET request"; 
    }
    
    json::value execute(const json::object& args) override {
        std::string url = std::string(args.at("url").as_string());
        
        // URL 白名单检查
        if (!is_allowed_url(url)) {
            json::object error;
            error["success"] = false;
            error["error"] = "URL not in whitelist";
            return error;
        }
        
        // 使用 Boost.Beast 发送 HTTP 请求（简化版）
        try {
            // 解析 URL
            auto [host, target] = parse_url(url);
            
            asio::io_context ioc;
            tcp::resolver resolver(ioc);
            beast::tcp_stream stream(ioc);
            
            auto const results = resolver.resolve(host, "80");
            stream.connect(results);
            
            http::request<http::string_body> req{http::verb::get, target, 11};
            req.set(http::field::host, host);
            req.set(http::field::user_agent, "NuClaw-Agent/1.0");
            
            http::write(stream, req);
            
            beast::flat_buffer buffer;
            http::response<http::dynamic_body> res;
            http::read(stream, buffer, res);
            
            beast::error_code ec;
            stream.socket().shutdown(tcp::socket::shutdown_both, ec);
            
            json::object result;
            result["success"] = true;
            result["status"] = res.result_int();
            result["body"] = beast::buffers_to_string(res.body().data());
            return result;
            
        } catch (const std::exception& e) {
            json::object error;
            error["success"] = false;
            error["error"] = std::string("HTTP request failed: ") + e.what();
            return error;
        }
    }

private:
    bool is_allowed_url(const std::string& url) {
        // 简化版：只允许 http，禁止访问内网
        if (url.find("http://") != 0) return false;
        if (url.find("localhost") != std::string::npos) return false;
        if (url.find("127.0.") != std::string::npos) return false;
        if (url.find("192.168.") != std::string::npos) return false;
        if (url.find("10.") != std::string::npos) return false;
        return true;
    }
    
    std::pair<std::string, std::string> parse_url(const std::string& url) {
        // 简化解析：假设 http://host/path 格式
        size_t host_start = url.find("://") + 3;
        size_t path_start = url.find('/', host_start);
        
        std::string host = url.substr(host_start, path_start - host_start);
        std::string target = (path_start == std::string::npos) 
            ? "/" 
            : url.substr(path_start);
        
        return {host, target};
    }
};

// ============ 文件操作工具（带沙箱）===========

class FileTool : public Tool {
public:
    FileTool() {
        // 默认只允许访问当前目录下的 data/ 文件夹
        sandbox_.allow_path(fs::current_path() / "data");
    }
    
    std::string get_name() const override { return "file"; }
    std::string get_description() const override { 
        return "File operations: read/write/list"; 
    }
    
    json::value execute(const json::object& args) override {
        std::string operation = std::string(args.at("operation").as_string());
        
        if (operation == "read") {
            return file_read(args);
        } else if (operation == "write") {
            return file_write(args);
        } else if (operation == "list") {
            return file_list(args);
        } else {
            json::object error;
            error["success"] = false;
            error["error"] = "Unknown operation: " + operation;
            return error;
        }
    }

private:
    Sandbox sandbox_;
    
    json::value file_read(const json::object& args) {
        std::string path = std::string(args.at("path").as_string());
        
        if (!Sandbox::is_safe_path(path)) {
            json::object error;
            error["success"] = false;
            error["error"] = "Path contains unsafe characters";
            return error;
        }
        
        if (!sandbox_.is_allowed(path)) {
            json::object error;
            error["success"] = false;
            error["error"] = "Access denied: path outside allowed directory";
            return error;
        }
        
        std::ifstream file(path);
        if (!file.is_open()) {
            json::object error;
            error["success"] = false;
            error["error"] = "Failed to open file: " + path;
            return error;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        
        json::object result;
        result["success"] = true;
        result["content"] = buffer.str();
        result["size"] = buffer.str().length();
        return result;
    }
    
    json::value file_write(const json::object& args) {
        std::string path = std::string(args.at("path").as_string());
        std::string content = std::string(args.at("content").as_string());
        
        if (!Sandbox::is_safe_path(path) || !sandbox_.is_allowed(path)) {
            json::object error;
            error["success"] = false;
            error["error"] = "Access denied";
            return error;
        }
        
        std::ofstream file(path);
        if (!file.is_open()) {
            json::object error;
            error["success"] = false;
            error["error"] = "Failed to create file";
            return error;
        }
        
        file << content;
        
        json::object result;
        result["success"] = true;
        result["bytes_written"] = content.length();
        return result;
    }
    
    json::value file_list(const json::object& args) {
        std::string path = args.contains("path") 
            ? std::string(args.at("path").as_string()) 
            : ".";
        
        if (!Sandbox::is_safe_path(path) || !sandbox_.is_allowed(path)) {
            json::object error;
            error["success"] = false;
            error["error"] = "Access denied";
            return error;
        }
        
        json::object result;
        result["success"] = true;
        json::array files;
        
        for (const auto& entry : fs::directory_iterator(path)) {
            json::object file_info;
            file_info["name"] = entry.path().filename().string();
            file_info["is_directory"] = entry.is_directory();
            file_info["size"] = entry.is_regular_file() 
                ? static_cast<int64_t>(entry.file_size()) 
                : 0;
            files.push_back(file_info);
        }
        
        result["files"] = files;
        return result;
    }
};

// ============ 代码执行工具（沙箱）===========

class CodeExecuteTool : public Tool {
public:
    std::string get_name() const override { return "code_execute"; }
    std::string get_description() const override { 
        return "Execute code in sandboxed environment (Python)"; 
    }
    
    json::value execute(const json::object& args) override {
        std::string code = std::string(args.at("code").as_string());
        std::string language = args.contains("language") 
            ? std::string(args.at("language").as_string()) 
            : "python";
        
        // 代码安全检查
        auto check = security_check(code, language);
        if (!check.first) {
            json::object error;
            error["success"] = false;
            error["error"] = "Security check failed: " + check.second;
            return error;
        }
        
        // 使用受限的 Python 解释器
        // 实际生产应该使用容器隔离
        std::string cmd = "timeout 5 python3 -c '" + escape_shell(code) + "' 2>&1";
        
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            json::object error;
            error["success"] = false;
            error["error"] = "Failed to execute code";
            return error;
        }
        
        char buffer[4096];
        std::string output;
        while (fgets(buffer, sizeof(buffer), pipe)) {
            output += buffer;
        }
        
        int status = pclose(pipe);
        
        json::object result;
        result["success"] = (status == 0);
        result["output"] = output;
        result["exit_code"] = status;
        return result;
    }

private:
    std::pair<bool, std::string> security_check(
        const std::string& code, 
        const std::string& language) {
        
        // 禁止的危险操作
        std::vector<std::string> blacklist = {
            "import os", "import sys", "__import__",
            "open(", "file(", "exec(", "eval(",
            "subprocess", "socket", "urllib",
            "rm -rf", "mkfs", "dd if"
        };
        
        for (const auto& bad : blacklist) {
            if (code.find(bad) != std::string::npos) {
                return {false, "Forbidden keyword: " + bad};
            }
        }
        
        // 限制代码长度
        if (code.length() > 10000) {
            return {false, "Code too long"};
        }
        
        return {true, ""};
    }
    
    std::string escape_shell(const std::string& s) {
        std::string result;
        for (char c : s) {
            if (c == '\'') result += "'\"'\"'";
            else result += c;
        }
        return result;
    }
};

// ============ 工具注册表 ============

class ToolRegistry {
public:
    void register_tool(std::shared_ptr<Tool> tool) {
        tools_[tool->get_name()] = tool;
    }
    
    std::shared_ptr<Tool> get_tool(const std::string& name) {
        auto it = tools_.find(name);
        if (it != tools_.end()) return it->second;
        return nullptr;
    }
    
    json::array list_tools() const {
        json::array arr;
        for (const auto& [name, tool] : tools_) {
            json::object info;
            info["name"] = name;
            info["description"] = tool->get_description();
            arr.push_back(info);
        }
        return arr;
    }

private:
    std::map<std::string, std::shared_ptr<Tool>> tools_;
};

// ============ WebSocket 服务器 ============

class Agent {
public:
    Agent() {
        registry_.register_tool(std::make_shared<HttpGetTool>());
        registry_.register_tool(std::make_shared<FileTool>());
        registry_.register_tool(std::make_shared<CodeExecuteTool>());
    }
    
    std::string process(const std::string& input) {
        try {
            auto json_input = json::parse(input);
            
            if (json_input.is_string() && json_input.as_string() == "list") {
                return json::serialize(registry_.list_tools());
            }
            
            auto obj = json_input.as_object();
            std::string tool_name = std::string(obj.at("tool").as_string());
            json::object args = obj.at("args").as_object();
            
            auto tool = registry_.get_tool(tool_name);
            if (!tool) {
                json::object error;
                error["success"] = false;
                error["error"] = "Tool not found: " + tool_name;
                return json::serialize(error);
            }
            
            auto result = tool->execute_safe(args);
            return json::serialize(result);
            
        } catch (const std::exception& e) {
            json::object error;
            error["success"] = false;
            error["error"] = e.what();
            return json::serialize(error);
        }
    }

private:
    ToolRegistry registry_;
};

class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    WsSession(tcp::socket socket, Agent& agent)
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
            
            std::string response = self->agent_.process(msg);
            
            self->ws_.text(true);
            self->ws_.async_write(asio::buffer(response),
                [self](beast::error_code, std::size_t) { self->do_read(); });
        });
    }
    
    websocket::stream<tcp::socket> ws_;
    Agent& agent_;
    beast::flat_buffer buffer_;
};

class Server {
public:
    Server(asio::io_context& io, short port, Agent& agent)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)), agent_(agent) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<WsSession>(std::move(socket), agent_)->start();
            }
            do_accept();
        });
    }
    
    tcp::acceptor acceptor_;
    Agent& agent_;
};

int main() {
    try {
        asio::io_context io;
        Agent agent;
        Server server(io, 8081, agent);
        
        std::cout << "NuClaw Step 8 - Tool Ecosystem\n";
        std::cout << "WebSocket: ws://localhost:8081\n\n";
        std::cout << "Available tools:\n";
        std::cout << "  - http_get: Send HTTP GET request\n";
        std::cout << "  - file: File operations (read/write/list)\n";
        std::cout << "  - code_execute: Execute Python code (sandboxed)\n\n";
        std::cout << "Send '\"list\"' to list all tools\n";
        
        // 创建 data 目录用于文件操作演示
        fs::create_directories("data");
        
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
