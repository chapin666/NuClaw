// src/step06/main.cpp
// Step 6: Tools 系统（上）- 同步工具、注册表、参数校验
// 编译: mkdir build && cd build && cmake .. && make
// 运行: ./nuclaw_step06

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <map>
#include <vector>
#include <memory>
#include <functional>
#include <stdexcept>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
namespace json = boost::json;
using tcp = asio::ip::tcp;

// ============ Tool Schema 定义 ============

// 参数类型
enum class ParamType {
    STRING,
    INTEGER,
    NUMBER,
    BOOLEAN,
    ARRAY,
    OBJECT
};

// 参数定义
struct Parameter {
    std::string name;
    ParamType type;
    std::string description;
    bool required = true;
    json::value default_value;  // 可选，仅当 required=false
};

// Tool Schema
struct ToolSchema {
    std::string name;
    std::string description;
    std::vector<Parameter> parameters;
};

// ============ 参数校验器 ============

class ParameterValidator {
public:
    static bool validate_type(const json::value& value, ParamType expected) {
        switch (expected) {
            case ParamType::STRING:
                return value.is_string();
            case ParamType::INTEGER:
                return value.is_int64();
            case ParamType::NUMBER:
                return value.is_number();
            case ParamType::BOOLEAN:
                return value.is_bool();
            case ParamType::ARRAY:
                return value.is_array();
            case ParamType::OBJECT:
                return value.is_object();
            default:
                return false;
        }
    }
    
    static std::string type_to_string(ParamType type) {
        switch (type) {
            case ParamType::STRING: return "string";
            case ParamType::INTEGER: return "integer";
            case ParamType::NUMBER: return "number";
            case ParamType::BOOLEAN: return "boolean";
            case ParamType::ARRAY: return "array";
            case ParamType::OBJECT: return "object";
            default: return "unknown";
        }
    }
    
    // 校验参数
    static std::vector<std::string> validate(
        const json::object& args,
        const std::vector<Parameter>& schema) {
        
        std::vector<std::string> errors;
        
        for (const auto& param : schema) {
            auto it = args.find(param.name);
            
            // 检查必需参数
            if (param.required && it == args.end()) {
                errors.push_back("Missing required parameter: " + param.name);
                continue;
            }
            
            // 如果有值，校验类型
            if (it != args.end()) {
                if (!validate_type(it->value(), param.type)) {
                    errors.push_back("Parameter '" + param.name + "' should be " +
                                   type_to_string(param.type));
                }
            }
        }
        
        // 检查未知参数
        for (const auto& kv : args) {
            bool found = false;
            for (const auto& param : schema) {
                if (param.name == kv.key()) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                errors.push_back("Unknown parameter: " + std::string(kv.key()));
            }
        }
        
        return errors;
    }
};

// ============ Tool 基类 ============

class Tool {
public:
    virtual ~Tool() = default;
    
    // 获取 Schema
    virtual ToolSchema get_schema() const = 0;
    
    // 执行工具（同步）
    virtual json::value execute(const json::object& args) = 0;
    
    // 带校验的执行
    json::value execute_safe(const json::object& args) {
        auto schema = get_schema();
        auto errors = ParameterValidator::validate(args, schema.parameters);
        
        if (!errors.empty()) {
            json::object error_result;
            error_result["success"] = false;
            error_result["error"] = "Validation failed";
            json::array error_list;
            for (const auto& e : errors) {
                error_list.push_back(e);
            }
            error_result["details"] = error_list;
            return error_result;
        }
        
        return execute(args);
    }
};

// ============ 具体工具实现 ============

// 计算器工具
class CalculatorTool : public Tool {
public:
    ToolSchema get_schema() const override {
        return {
            "calculator",
            "执行数学计算",
            {
                {"expression", ParamType::STRING, "数学表达式，如 1+2*3", true},
                {"precision", ParamType::INTEGER, "结果精度（小数位）", false, 2}
            }
        };
    }
    
    json::value execute(const json::object& args) override {
        std::string expr = args.at("expression").as_string();
        
        // 简化版：只支持基本运算
        double result = evaluate_simple(expr);
        
        int precision = 2;
        if (args.contains("precision")) {
            precision = static_cast<int>(args.at("precision").as_int64());
        }
        
        json::object result_obj;
        result_obj["success"] = true;
        result_obj["expression"] = expr;
        result_obj["result"] = std::round(result * std::pow(10, precision)) / std::pow(10, precision);
        return result_obj;
    }

private:
    double evaluate_simple(const std::string& expr) {
        // 简化实现：这里应该使用表达式解析器
        // 为了演示，只处理简单的数字
        try {
            return std::stod(expr);
        } catch (...) {
            return 0.0;
        }
    }
};

// 文件读取工具
class FileReadTool : public Tool {
public:
    ToolSchema get_schema() const override {
        return {
            "file_read",
            "读取文件内容",
            {
                {"path", ParamType::STRING, "文件路径", true},
                {"limit", ParamType::INTEGER, "最大读取行数", false, 100}
            }
        };
    }
    
    json::value execute(const json::object& args) override {
        std::string path = args.at("path").as_string();
        
        // 安全检查：禁止访问上级目录
        if (path.find("..") != std::string::npos) {
            json::object error;
            error["success"] = false;
            error["error"] = "Access denied: path contains '..'";
            return error;
        }
        
        int limit = 100;
        if (args.contains("limit")) {
            limit = static_cast<int>(args.at("limit").as_int64());
            limit = std::min(limit, 1000);  // 最大 1000 行
        }
        
        // 读取文件
        std::ifstream file(path);
        json::object result;
        
        if (!file.is_open()) {
            result["success"] = false;
            result["error"] = "Failed to open file: " + path;
            return result;
        }
        
        std::string content;
        std::string line;
        int lines = 0;
        while (std::getline(file, line) && lines < limit) {
            content += line + "\n";
            lines++;
        }
        
        result["success"] = true;
        result["path"] = path;
        result["content"] = content;
        result["lines"] = lines;
        return result;
    }
};

// 系统信息工具
class SystemInfoTool : public Tool {
public:
    ToolSchema get_schema() const override {
        return {
            "system_info",
            "获取系统信息",
            {
                {"type", ParamType::STRING, "信息类型: time/memory/cpu", true}
            }
        };
    }
    
    json::value execute(const json::object& args) override {
        std::string type = args.at("type").as_string();
        json::object result;
        result["success"] = true;
        result["type"] = type;
        
        if (type == "time") {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
            result["time"] = ss.str();
            result["timestamp"] = static_cast<int64_t>(time);
        }
        else if (type == "memory") {
            // 简化版，实际应该读取 /proc/meminfo
            result["usage"] = "N/A (demo)";
        }
        else if (type == "cpu") {
            result["cores"] = static_cast<int>(std::thread::hardware_concurrency());
        }
        else {
            result["success"] = false;
            result["error"] = "Unknown info type: " + type;
        }
        
        return result;
    }
};

// ============ 工具注册表 ============

class ToolRegistry {
public:
    using ToolPtr = std::shared_ptr<Tool>;
    
    // 注册工具
    void register_tool(ToolPtr tool) {
        auto schema = tool->get_schema();
        tools_[schema.name] = tool;
        schemas_[schema.name] = schema;
    }
    
    // 获取工具
    ToolPtr get_tool(const std::string& name) {
        auto it = tools_.find(name);
        if (it != tools_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    // 获取所有 Schema（用于 LLM 了解可用工具）
    json::array get_all_schemas() const {
        json::array arr;
        for (const auto& [name, schema] : schemas_) {
            arr.push_back(schema_to_json(schema));
        }
        return arr;
    }
    
    // 列出所有工具名称
    std::vector<std::string> list_tools() const {
        std::vector<std::string> names;
        for (const auto& [name, _] : tools_) {
            names.push_back(name);
        }
        return names;
    }
    
    // 执行工具调用
    json::value execute(const std::string& tool_name, const json::object& args) {
        auto tool = get_tool(tool_name);
        if (!tool) {
            json::object error;
            error["success"] = false;
            error["error"] = "Tool not found: " + tool_name;
            return error;
        }
        return tool->execute_safe(args);
    }

private:
    std::map<std::string, ToolPtr> tools_;
    std::map<std::string, ToolSchema> schemas_;
    
    json::object schema_to_json(const ToolSchema& schema) {
        json::object obj;
        obj["name"] = schema.name;
        obj["description"] = schema.description;
        
        json::object params;
        for (const auto& param : schema.parameters) {
            json::object p;
            p["type"] = ParameterValidator::type_to_string(param.type);
            p["description"] = param.description;
            p["required"] = param.required;
            if (!param.required && !param.default_value.is_null()) {
                p["default"] = param.default_value;
            }
            params[param.name] = p;
        }
        obj["parameters"] = params;
        return obj;
    }
};

// ============ Agent Loop ============

class AgentLoop {
public:
    AgentLoop() {
        // 注册默认工具
        registry_.register_tool(std::make_shared<CalculatorTool>());
        registry_.register_tool(std::make_shared<FileReadTool>());
        registry_.register_tool(std::make_shared<SystemInfoTool>());
    }
    
    std::string process(const std::string& input) {
        // 解析工具调用请求（简化版 JSON 解析）
        try {
            auto json_input = json::parse(input);
            
            if (json_input.is_object()) {
                auto obj = json_input.as_object();
                std::string tool_name = std::string(obj.at("tool").as_string());
                json::object args;
                if (obj.contains("args")) {
                    args = obj.at("args").as_object();
                }
                
                auto result = registry_.execute(tool_name, args);
                return json::serialize(result);
            }
        } catch (const std::exception& e) {
            json::object error;
            error["success"] = false;
            error["error"] = "Parse error: " + std::string(e.what());
            return json::serialize(error);
        }
        
        // 非工具调用，返回帮助信息
        json::object help;
        help["message"] = "Send JSON: {\"tool\": \"name\", \"args\": {...}}";
        help["available_tools"] = registry_.get_all_schemas();
        return json::serialize(help);
    }

private:
    ToolRegistry registry_;
};

// ============ WebSocket 服务器 ============

class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    WsSession(tcp::socket socket, AgentLoop& agent)
        : ws_(std::move(socket)), agent_(agent) {}
    
    void start() {
        ws_.async_accept(
            [self = shared_from_this()](beast::error_code ec) {
                if (!ec) self->do_read();
            });
    }

private:
    void do_read() {
        ws_.async_read(buffer_,
            [self = shared_from_this()](beast::error_code ec, std::size_t) {
                if (ec) return;
                
                std::string msg = beast::buffers_to_string(self->buffer_.data());
                self->buffer_.consume(self->buffer_.size());
                
                std::string response = self->agent_.process(msg);
                
                self->ws_.text(true);
                self->ws_.async_write(asio::buffer(response),
                    [self](beast::error_code, std::size_t) {
                        self->do_read();
                    });
            });
    }
    
    websocket::stream<tcp::socket> ws_;
    AgentLoop& agent_;
    beast::flat_buffer buffer_;
};

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

// ============ 主函数 ============

int main() {
    try {
        asio::io_context io;
        AgentLoop agent;
        Server server(io, 8081, agent);
        
        std::cout << "NuClaw Step 6 - Tool System (Sync)\n";
        std::cout << "WebSocket: ws://localhost:8081\n";
        std::cout << "\nAvailable tools:\n";
        std::cout << "  - calculator: Math calculation\n";
        std::cout << "  - file_read: Read file content\n";
        std::cout << "  - system_info: Get system info\n";
        std::cout << "\nExample: {\"tool\": \"calculator\", \"args\": {\"expression\": \"1+2\"}}\n";
        
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
