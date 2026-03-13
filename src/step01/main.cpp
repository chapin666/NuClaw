// ============================================================================
// Step 1: 异步 I/O 服务器
// ============================================================================
// 演进说明：
//   基于 Step 0 演进为异步版本
//   1. 新增 Session 类管理单个连接生命周期
//   2. 使用 async_accept/async_read/async_write 替代同步调用
//   3. 多线程运行 io_context 提高并发能力
//   4. 保留 io_context, acceptor, endpoint 核心概念
// 编译: g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
// 运行: ./server
// 测试: curl http://localhost:8080
// ============================================================================

#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// 简化命名空间
using boost::asio::ip::tcp;
namespace asio = boost::asio;

// ============================================================================
// 新增：Session 类 - 管理单个连接的生命周期
// ============================================================================
// 为什么需要 Session？
//   - 异步编程中，操作完成前连接必须保持活跃
//   - enable_shared_from_this 允许在回调中安全地保持对象存活
//   - 每个 Session 独立处理一个客户端连接
// ============================================================================
class Session : public std::enable_shared_from_this<Session> {
public:
    // 构造函数：接收已连接的 socket
    explicit Session(tcp::socket socket)
        : socket_(std::move(socket)) {}  // 移动语义转移 socket 所有权
    
    // 启动 Session：开始异步读取
    void start() {
        do_read();
    }

private:
    // ============================================================================
    // 异步读取：非阻塞读取客户端数据
    // ============================================================================
    void do_read() {
        // shared_from_this() 关键：确保回调执行时 Session 对象仍存在
        // 回调捕获 self，引用计数 +1，防止对象提前销毁
        auto self = shared_from_this();
        
        socket_.async_read_some(
            asio::buffer(buffer_, sizeof(buffer_)),
            [this, self](boost::system::error_code ec, std::size_t len) {
                if (!ec) {
                    std::cout << "[+] Received " << len << " bytes" << std::endl;
                    
                    // 构造响应并发送
                    auto response = make_response();
                    do_write(response);
                }
                // 如果出错（如连接关闭），Session 对象会被自动销毁
            }
        );
    }
    
    // ============================================================================
    // 异步写入：非阻塞发送响应给客户端
    // ============================================================================
    void do_write(const std::string& response) {
        auto self = shared_from_this();
        
        // async_write 确保完整发送（内部会处理分包）
        asio::async_write(socket_, asio::buffer(response),
            [this, self](boost::system::error_code, std::size_t) {
                std::cout << "[+] Response sent, closing connection" << std::endl;
                // socket 析构时自动关闭连接
            }
        );
    }
    
    // 构造 HTTP 响应（与 Step 0 相同）
    std::string make_response() {
        std::string body = R"({
    "status": "ok",
    "step": 1,
    "message": "Hello from NuClaw Async Server!",
    "note": "Using async I/O with Session class"
})";
        
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: " + std::to_string(body.length()) + "\r\n"
               "\r\n" + body;
    }
    
    tcp::socket socket_;           // TCP 连接 socket
    char buffer_[1024] = {};       // 读取缓冲区
};

// ============================================================================
// Server 类 - 监听端口并接受连接（保留 Step 0 核心概念）
// ============================================================================
class Server {
public:
    Server(asio::io_context& io, short port)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)) {  // 保留 endpoint 概念
        std::cout << "[i] Server binding to port " << port << std::endl;
        do_accept();  // 开始异步接受连接
    }

private:
    // ============================================================================
    // 异步接受连接
    // ============================================================================
    void do_accept() {
        // async_accept: 非阻塞等待新连接
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::cout << "[+] New connection accepted" << std::endl;
                    
                    // 创建 Session 处理新连接
                    // shared_ptr 管理 Session 生命周期，自动处理内存
                    std::make_shared<Session>(std::move(socket))->start();
                }
                // 继续接受下一个连接（关键！）
                do_accept();
            }
        );
    }
    
    tcp::acceptor acceptor_;       // 保留 acceptor 概念，用于监听连接
};

int main() {
    try {
        // 保留 io_context 概念（Step 0 的核心）
        asio::io_context io;
        
        std::cout << "========================================" << std::endl;
        std::cout << "  NuClaw Step 1 - Async I/O Server" << std::endl;
        std::cout << "========================================" << std::endl;
        
        // 创建服务器（开始监听）
        Server server(io, 8080);
        
        std::cout << "Server listening on http://localhost:8080" << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        std::cout << std::endl;
        
        // ============================================================================
        // 新增：多线程运行 io_context
        // ============================================================================
        // 为什么需要多线程？
        //   - 单线程 io.run() 只能利用一个 CPU 核心
        //   - 多线程可以同时处理多个并发连接
        //   - Asio 线程安全：多个线程可同时调用 io.run()
        // ============================================================================
        std::vector<std::thread> threads;
        auto thread_count = std::thread::hardware_concurrency();
        
        std::cout << "[i] Starting " << thread_count << " worker threads..." << std::endl;
        
        for (unsigned i = 0; i < thread_count; ++i) {
            threads.emplace_back([&io]() {
                io.run();  // 每个线程都运行事件循环
            });
        }
        
        // 等待所有线程结束
        for (auto& t : threads) {
            t.join();
        }
        
    } catch (std::exception& e) {
        std::cerr << "[!] Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
