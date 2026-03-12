// src/step00/main.cpp
// Step 0: HTTP Echo 服务器
// 
// 这是 NuClaw 教程的第一章，用约 50 行代码实现最简单的 HTTP Echo 服务器。
// 目标：理解 Boost.Asio 基础概念，掌握同步 TCP 服务器编写。
//
// 编译: g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
// 运行: ./server
// 测试: curl http://localhost:8080

#include <boost/asio.hpp>
#include <iostream>
#include <string>

// 简化命名空间，避免重复输入 boost::asio::ip
using boost::asio::ip::tcp;

// 构造 HTTP 响应
// 参数: body - JSON 响应体
// 返回: 完整 HTTP 响应字符串，符合 HTTP/1.1 协议格式
std::string make_response(const std::string& body) {
    // HTTP 响应格式：状态行 + 头部 + 空行 + 主体
    return "HTTP/1.1 200 OK\r\n"                           // 状态行
           "Content-Type: application/json\r\n"            // 内容类型
           "Content-Length: " + std::to_string(body.length()) + "\r\n"  // 内容长度（必须准确）
           "\r\n"                                          // 空行分隔头部和主体
           + body;                                         // 响应主体
}

int main() {
    try {
        // io_context: Asio 的核心，管理所有 I/O 操作的事件循环
        // 它负责调度异步操作，但在本章中我们使用同步模式
        boost::asio::io_context io;
        
        // acceptor: 监听指定端口的 TCP 连接
        // tcp::v4(): 使用 IPv4 协议
        // 8080: 监听的端口号
        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 8080));
        
        std::cout << "========================================" << std::endl;
        std::cout << "  NuClaw Step 0 - HTTP Echo Server" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Server listening on http://localhost:8080" << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        std::cout << std::endl;
        
        // 主循环：持续接受客户端连接
        // 这是同步阻塞模式，一次只能处理一个连接
        while (true) {
            // socket: 代表一个 TCP 连接
            // 每个连接有独立的 socket 对象
            tcp::socket socket(io);
            
            // accept(): 阻塞等待客户端连接
            // 有连接到达时，socket 会被赋值，程序继续执行
            acceptor.accept(socket);
            
            std::cout << "[+] New connection accepted" << std::endl;
            
            // 读取客户端发送的 HTTP 请求
            // 简化版：只读取前 1024 字节
            // 实际 HTTP 请求可能更长，生产环境需要循环读取
            char buffer[1024] = {};
            size_t len = socket.read_some(boost::asio::buffer(buffer));
            
            std::cout << "[+] Received " << len << " bytes" << std::endl;
            
            // 构造 JSON 响应体
            // 使用原始字符串字面量 R"(...)", 避免转义问题
            std::string body = R"({
    "status": "ok",
    "step": 0,
    "message": "Hello from NuClaw!",
    "note": "This is the simplest HTTP server"
})";
            
            // 构造完整 HTTP 响应
            auto response = make_response(body);
            
            // 发送响应给客户端
            boost::asio::write(socket, boost::asio::buffer(response));
            
            std::cout << "[+] Response sent" << std::endl;
            
            // socket 在这里析构，连接自动关闭
            // 这是短连接模式，每个请求后关闭连接
            // Step 2 会实现长连接优化
        }
        
    } catch (std::exception& e) {
        // 异常处理：打印错误信息
        // 生产环境应该有更完善的日志系统
        std::cerr << "[!] Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

// 延伸思考（课后作业）：
// 1. 如果同时有 100 个客户端请求，这个服务器会怎样？
//    答案：串行处理，后面的客户端会阻塞等待
// 
// 2. 如何实现 HTTP 请求解析（获取 Path、Headers）？
//    提示：解析 buffer 中的 HTTP 请求行和头部
//
// 3. 如何添加路由（不同 Path 返回不同内容）？
//    提示：根据解析的 Path 分发到不同处理函数
//
// 这些问题将在 Step 1-2 中解答
