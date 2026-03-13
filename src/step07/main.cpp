// src/step07/main.cpp
// Step 7: Tools 系统（中）- 异步工具执行、超时控制、并发限制

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <map>
#include <vector>
#include <memory>
#include <queue>
#include <future>
#include <chrono>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
namespace json = boost::json;
using tcp = asio::ip::tcp;
using namespace std::chrono_literals;

// Tool 基类（支持异步）
class Tool {
public:
    virtual ~Tool() = default;
    virtual std::string get_name() const = 0;
    
    // 异步执行接口
    virtual void execute_async(const json::object& args,
                               std::function<void(json::value)> callback) = 0;
    
    // 同步包装（用于简单场景）
    json::value execute_sync(const json::object& args) {
        std::promise<json::value> promise;
        auto future = promise.get_future();
        
        execute_async(args, [&promise](json::value result) {
            promise.set_value(std::move(result));
        });
        
        return future.get();
    }
};

// 异步 HTTP 请求工具
class HttpTool : public Tool {
public:
    std::string get_name() const override { return "http_request"; }
    
    void execute_async(const json::object& args,
                       std::function<void(json::value)> callback) override {
        // 简化版：实际应该使用 Boost.Beast 实现完整 HTTP 客户端
        std::string url = std::string(args.at("url").as_string());
        std::string method = args.contains("method") 
            ? std::string(args.at("method").as_string()) 
            : "GET";
        
        // 模拟异步 HTTP 请求
        std::thread([url, method, callback]() {
            std::this_thread::sleep_for(100ms);  // 模拟网络延迟
            
            json::object result;
            result["success"] = true;
            result["url"] = url;
            result["method"] = method;
            result["status"] = 200;
            result["body"] = "{\"message\": \"Mock response from " + url + "\"}";
            
            callback(result);
        }).detach();
    }
};

// 异步数据库查询工具（模拟）
class DatabaseTool : public Tool {
public:
    std::string get_name() const override { return "database_query"; }
    
    void execute_async(const json::object& args,
                       std::function<void(json::value)> callback) override {
        std::string query = std::string(args.at("sql").as_string());
        int timeout_ms = args.contains("timeout") 
            ? static_cast<int>(args.at("timeout").as_int64()) 
            : 5000;
        
        std::thread([query, timeout_ms, callback]() {
            auto start = std::chrono::steady_clock::now();
            
            // 模拟查询
            std::this_thread::sleep_for(50ms);
            
            auto elapsed = std::chrono::steady_clock::now() - start;
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
            
            if (ms > timeout_ms) {
                json::object error;
                error["success"] = false;
                error["error"] = "Query timeout";
                callback(error);
            } else {
                json::object result;
                result["success"] = true;
                result["query"] = query;
                result["rows"] = 42;
                result["time_ms"] = ms;
                callback(result);
            }
        }).detach();
    }
};

// 工具执行器（带并发控制）
class ToolExecutor {
public:
    ToolExecutor(size_t max_concurrent = 4) 
        : max_concurrent_(max_concurrent), running_(0) {}
    
    void submit(std::shared_ptr<Tool> tool, 
                const json::object& args,
                std::function<void(json::value)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (running_ >= max_concurrent_) {
            // 加入等待队列
            queue_.push({tool, args, callback});
        } else {
            running_++;
            execute_internal(tool, args, callback);
        }
    }
    
    void wait_all() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return running_ == 0 && queue_.empty(); });
    }

private:
    struct Task {
        std::shared_ptr<Tool> tool;
        json::object args;
        std::function<void(json::value)> callback;
    };
    
    void execute_internal(std::shared_ptr<Tool> tool,
                          const json::object& args,
                          std::function<void(json::value)> callback) {
        tool->execute_async(args, [this, callback](json::value result) {
            callback(std::move(result));
            on_complete();
        });
    }
    
    void on_complete() {
        std::lock_guard<std::mutex> lock(mutex_);
        running_--;
        
        if (!queue_.empty()) {
            auto task = queue_.front();
            queue_.pop();
            running_++;
            
            // 解锁后执行
            lock.~lock_guard();
            execute_internal(task.tool, task.args, task.callback);
        } else {
            cv_.notify_all();
        }
    }
    
    size_t max_concurrent_;
    size_t running_;
    std::queue<Task> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

// Agent Loop
class AgentLoop {
public:
    AgentLoop() : executor_(4) {
        register_tool(std::make_shared<HttpTool>());
        register_tool(std::make_shared<DatabaseTool>());
    }
    
    void register_tool(std::shared_ptr<Tool> tool) {
        tools_[tool->get_name()] = tool;
    }
    
    void execute_async(const std::string& tool_name,
                       const json::object& args,
                       std::function<void(json::value)> callback) {
        auto it = tools_.find(tool_name);
        if (it == tools_.end()) {
            json::object error;
            error["success"] = false;
            error["error"] = "Tool not found: " + tool_name;
            callback(error);
            return;
        }
        
        executor_.submit(it->second, args, callback);
    }
    
    void wait_all() {
        executor_.wait_all();
    }

private:
    std::map<std::string, std::shared_ptr<Tool>> tools_;
    ToolExecutor executor_;
};

// WebSocket 服务器
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
            
            try {
                auto json_input = json::parse(msg);
                auto obj = json_input.as_object();
                std::string tool_name = std::string(obj.at("tool").as_string());
                json::object args = obj.at("args").as_object();
                
                // 异步执行工具
                self->agent_.execute_async(tool_name, args, 
                    [self](json::value result) {
                        self->ws_.text(true);
                        self->ws_.async_write(asio::buffer(json::serialize(result)),
                            [self](beast::error_code, std::size_t) {
                                self->do_read();
                            });
                    });
            } catch (const std::exception& e) {
                json::object error;
                error["success"] = false;
                error["error"] = e.what();
                self->ws_.text(true);
                self->ws_.async_write(asio::buffer(json::serialize(error)),
                    [self](beast::error_code, std::size_t) {
                        self->do_read();
                    });
            }
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
            if (!ec) {
                std::make_shared<WsSession>(std::move(socket), agent_)->start();
            }
            do_accept();
        });
    }
    
    tcp::acceptor acceptor_;
    AgentLoop& agent_;
};

int main() {
    try {
        asio::io_context io;
        AgentLoop agent;
        Server server(io, 8081, agent);
        
        std::cout << "NuClaw Step 7 - Async Tool System\n";
        std::cout << "WebSocket: ws://localhost:8081\n";
        std::cout << "Features: Async execution, Timeout, Concurrency limit\n";
        
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
