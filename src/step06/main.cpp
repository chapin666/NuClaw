// ============================================================================
// Step 6: Tools 系统（上）- 同步工具、注册表、参数校验
// ============================================================================
//
// 本章核心：构建 Tool 系统的基础框架
//
// 核心组件：
//   +-------------------------------------------------------------+
//   |  ParamType                                                   |
//   |  Parameter -- 描述工具参数（名称、类型、必填）               |
//   |  ToolSchema -- 工具的定义（元数据）                          |
//   |       |                                                      |
//   |       v                                                      |
//   |  ParameterValidator -- 参数类型校验                          |
//   |       |                                                      |
//   |       v                                                      |
//   |  Tool（抽象基类）-- execute()                                 |
//   |       |                                                      |
//   |       +-- CalculatorTool -- 数学计算                         |
//   |       +-- FileReadTool -- 文件读取（带安全检查）             |
//   |       +-- SystemInfoTool -- 系统信息                         |
//   |       |                                                      |
//   |       v                                                      |
//   |  ToolRegistry -- 工具注册表（名称->工具映射）                |
//   |       |                                                      |
//   |       v                                                      |
//   |  AgentLoop -- 整合工具系统到 Agent                           |
//   +-------------------------------------------------------------+
//
// 关键概念：
//   - Schema：工具的"说明书"，让 LLM 知道如何使用
//   - 校验：在工具执行前验证参数，提前发现错误
//   - 注册表：运行时动态管理工具，支持扩展
//
// 编译: mkdir build && cd build && cmake .. && make
// 运行: ./nuclaw_step06
// 测试: wscat -c ws://localhost:8081
// ============================================================================

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <map>
#include <vector>
#include <memory>
#include <functional>
#include <stdexcept>
#include <fstream>
#include <cmath>
#include <chrono>
#include <iomanip>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
namespace json = boost::json;
using tcp = asio::ip::tcp;

// ----------------------------------------------------------------------------
// 参数类型枚举
// ----------------------------------------------------------------------------
// 为什么用 enum class 而不是 enum？
// - 类型安全：ParamType::STRING 不会隐式转为 int
// - 作用域隔离：避免命名冲突
// ----------------------------------------------------------------------------
enum class ParamType {
    STRING,     // JSON 字符串
    INTEGER,    // 整数（int64）
    NUMBER,     // 数字（整数或浮点）
    BOOLEAN,    // true/false
    ARRAY,      // JSON 数组
    OBJECT      // JSON 对象
};

// ----------------------------------------------------------------------------
// 参数定义
// ----------------------------------------------------------------------------
// 描述一个参数的所有元信息
// LLM 使用这些信息了解工具需要什么参数
// ----------------------------------------------------------------------------
struct Parameter {
    std::string name;           // 参数名（如 "expression"）
    ParamType type;             // 参数类型
    std::string description;    // 描述（给 LLM 看的）
    bool required = true;       // 是否必填
    json::value default_value;  // 可选参数的默认值
    
    // 构造函数简化创建
    Parameter(std::string n, ParamType t, std::string d, bool r = true, json::value def = {})
        : name(std::move(n)), type(t), description(std::move(d)), required(r), default_value(def) {}
};

// ----------------------------------------------------------------------------
// Tool Schema - 工具的定义
// ----------------------------------------------------------------------------
// 相当于工具的"API 文档"
// 用于：
//   1. LLM 了解有哪些工具可用
//   2. 自动生成工具调用 JSON
//   3. 参数校验的依据
// ----------------------------------------------------------------------------
struct ToolSchema {
    std::string name;                   // 工具名（唯一标识）
    std::string description;            // 工具功能描述
    std::vector<Parameter> parameters;  // 参数列表
};

// ----------------------------------------------------------------------------
// 参数校验器
// ----------------------------------------------------------------------------
// 职责：在工具执行前验证参数是否符合 Schema
// 好处：提前发现错误，提供清晰的错误信息
// ----------------------------------------------------------------------------
class ParameterValidator {
public:
    // 校验单个值的类型
    static bool validate_type(const json::value& value, ParamType expected) {
        switch (expected) {
            case ParamType::STRING:   return value.is_string();
            case ParamType::INTEGER:  return value.is_int64();
            case ParamType::NUMBER:   return value.is_number();
            case ParamType::BOOLEAN:  return value.is_bool();
            case ParamType::ARRAY:    return value.is_array();
            case ParamType::OBJECT:   return value.is_object();
            default: return false;
        }
    }
    
    // 类型转字符串（用于错误提示）
    static std::string type_to_string(ParamType type) {
        switch (type) {
            case ParamType::STRING:   return "string";
            case ParamType::INTEGER:  return "integer";
            case ParamType::NUMBER:   return "number";
            case ParamType::BOOLEAN:  return "boolean";
            case ParamType::ARRAY:    return "array";
            case ParamType::OBJECT:   return "object";
            default: return "unknown";
        }
    }
    
    // 完整校验：检查所有参数
    static std::vector<std::string> validate(
        const json::object& args,
        const std::vector<Parameter>& schema) {
        
        std::vector<std::string> errors;
        
        // 1. 检查必需参数是否缺失
        for (const auto& param : schema) {
            auto it = args.find(param.name);
            if (param.required && it == args.end()) {
                errors.push_back("Missing required parameter: " + param.name);
                continue;
            }
            
            // 2. 检查参数类型
            if (it != args.end() && !validate_type(it->value(), param.type)) {
                errors.push_back("Parameter '" + param.name + "' should be " +
                               type_to_string(param.type) + ", got " +
                               json::serialize(it->value()));
            }
        }
        
        // 3. 检查是否有未知参数（防止拼写错误）
        for (const auto& kv : args) {
            bool found = false;
            for (const auto& param : schema) {
                if (param.name == kv.key()) { found = true; break; }
            }
            if (!found) {
                errors.push_back("Unknown parameter: " + std::string(kv.key()));
            }
        }
        
        return errors;
    }
};

// ----------------------------------------------------------------------------
// Tool 抽象基类
// ----------------------------------------------------------------------------
// 所有工具的基类，定义统一接口
// 子类只需要实现：get_schema() 和 execute()
// ----------------------------------------------------------------------------
class Tool {
public:
    virtual ~Tool() = default;
    
    // 获取工具定义（必须实现）
    virtual ToolSchema get_schema() const = 0;
    
    // 执行工具（必须实现）
    virtual json::value execute(const json::object& args) = 0;
    
    // 带校验的执行（基类提供）
    json::value execute_safe(const json::object& args) {
        auto schema = get_schema();
        auto errors = ParameterValidator::validate(args, schema.parameters);
        
        if (!errors.empty()) {
            json::object error_result;
            error_result["success"] = false;
            error_result["error"] = "Validation failed";
            json::array error_list;
            for (const auto& e : errors) error_list.push_back(e);
            error_result["details"] = error_list;
            return error_result;
        }
        
        return execute(args);
    }
};

// ============================================================================
// 具体工具实现
// ============================================================================

// ----------------------------------------------------------------------------
// 计算器工具
// ----------------------------------------------------------------------------
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
        double result = evaluate_simple(expr);
        
        int precision = args.contains("precision") 
            ? static_cast<int>(args.at("precision").as_int64()) 
            : 2;
        
        json::object result_obj;
        result_obj["success"] = true;
        result_obj["expression"] = expr;
        result_obj["result"] = std::round(result * std::pow(10, precision)) / std::pow(10, precision);
        return result_obj;
    }

private:
    double evaluate_simple(const std::string& expr) {
        // 简化版：实际应该使用表达式解析器（如 exprtk）
        try { return std::stod(expr); } catch (...) { return 0.0; }
    }
};

// ----------------------------------------------------------------------------
// 文件读取工具（带安全检查）
// ----------------------------------------------------------------------------
class FileReadTool : public Tool {
public:
    ToolSchema get_schema() const override {
        return {
            "file_read",
            "读取文件内容（限制访问范围）",
            {
                {"path", ParamType::STRING, "文件路径（相对路径，禁止 ..）", true},
                {"limit", ParamType::INTEGER, "最大读取行数（默认100，最大1000）", false, 100}
            }
        };
    }
    
    json::value execute(const json::object& args) override {
        std::string path = args.at("path").as_string();
        
        // 安全检查：防止路径遍历攻击
        if (path.find("..") != std::string::npos || path[0] == '/') {
            json::object error;
            error["success"] = false;
            error["error"] = "Access denied: invalid path";
            return error;
        }
        
        int limit = 100;
        if (args.contains("limit")) {
            limit = static_cast<int>(args.at("limit").as_int64());
            limit = std::min(limit, 1000);
        }
        
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

// ----------------------------------------------------------------------------
// 系统信息工具
// ----------------------------------------------------------------------------
class SystemInfoTool : public Tool {
public:
    ToolSchema get_schema() const override {
        return {
            "system_info",
            "获取系统信息",
            {{"type", ParamType::STRING, "信息类型: time/memory/cpu", true}}
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
        else if (type == "cpu") {
            result["cores"] = static_cast<int>(std::thread::hardware_concurrency());
        }
        else {
            result["success"] = false;
            result["error"] = "Unknown type: " + type;
        }
        
        return result;
    }
};

// ============================================================================
// 工具注册表
// ============================================================================

class ToolRegistry {
public:
    using ToolPtr = std::shared_ptr<Tool>;
    
    void register_tool(ToolPtr tool) {
        auto schema = tool->get_schema();
        tools_[schema.name] = tool;
        schemas_[schema.name] = schema;
        std::cout << "[+] Tool registered: " << schema.name << "\n";
    }
    
    ToolPtr get_tool(const std::string& name) {
        auto it = tools_.find(name);
        return (it != tools_.end()) ? it->second : nullptr;
    }
    
    json::array get_all_schemas() const {
        json::array arr;
        for (const auto& [name, schema] : schemas_) {
            arr.push_back(schema_to_json(schema));
        }
        return arr;
    }
    
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
        for (const auto& p : schema.parameters) {
            json::object param_obj;
            param_obj["type"] = ParameterValidator::type_to_string(p.type);
            param_obj["description"] = p.description;
            param_obj["required"] = p.required;
            params[p.name] = param_obj;
        }
        obj["parameters"] = params;
        return obj;
    }
};

// ============================================================================
// Agent & Server
// ============================================================================

class AgentLoop {
public:
    AgentLoop() {
        registry_.register_tool(std::make_shared<CalculatorTool>());
        registry_.register_tool(std::make_shared<FileReadTool>());
        registry_.register_tool(std::make_shared<SystemInfoTool>());
    }
    
    std::string process(const std::string& input) {
        try {
            auto json_input = json::parse(input);
            
            if (json_input.is_object()) {
                auto obj = json_input.as_object();
                std::string tool_name = std::string(obj.at("tool").as_string());
                json::object args = obj.contains("args") ? obj.at("args").as_object() : json::object{};
                
                std::cout << "[⚙️ Executing] " << tool_name << "\n";
                auto result = registry_.execute(tool_name, args);
                return json::serialize(result);
            }
        } catch (const std::exception& e) {
            json::object error;
            error["success"] = false;
            error["error"] = "Parse error: " + std::string(e.what());
            return json::serialize(error);
        }
        
        json::object help;
        help["message"] = "Send JSON: {\"tool\": \"name\", \"args\": {...}}";
        help["available_tools"] = registry_.get_all_schemas();
        return json::serialize(help);
    }

private:
    ToolRegistry registry_;
};

// WebSocket Session
class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    WsSession(tcp::socket socket, AgentLoop& agent)
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
            
            std::cout << "[<] " << msg << "\n";
            std::string response = self->agent_.process(msg);
            
            self->ws_.text(true);
            self->ws_.async_write(asio::buffer(response),
                [self](beast::error_code, std::size_t) { self->do_read(); });
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
        acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) std::make_shared<WsSession>(std::move(socket), agent_)->start();
            do_accept();
        });
    }
    
    tcp::acceptor acceptor_;
    AgentLoop& agent_;
};

// ============================================================================
// Main
// ============================================================================
int main() {
    try {
        std::cout << "========================================\n";
        std::cout << "  NuClaw Step 6 - Tool System (Sync)\n";
        std::cout << "========================================\n\n";
        
        asio::io_context io;
        AgentLoop agent;
        Server server(io, 8081, agent);
        
        std::cout << "[✓] WebSocket server started on ws://localhost:8081\n\n";
        std::cout << "Example requests:\n";
        std::cout << "  {\"tool\": \"calculator\", \"args\": {\"expression\": \"3.14\"}}\n";
        std::cout << "  {\"tool\": \"system_info\", \"args\": {\"type\": \"time\"}}\n\n";
        
        std::vector<std::thread> threads;
        for (unsigned i = 0; i < std::thread::hardware_concurrency(); ++i) {
            threads.emplace_back([&io]() { io.run(); });
        }
        for (auto& t : threads) t.join();
        
    } catch (std::exception& e) {
        std::cerr << "[✗] Error: " << e.what() << std::endl;
    }
    return 0;
}
