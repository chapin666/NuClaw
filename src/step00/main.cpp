// src/step00/main.cpp
// Step 0: 最简单的 HTTP Echo 服务器
// 编译: g++ -std=c++17 main.cpp -o server -lboost_system -lpthread
// 运行: ./server
// 测试: curl http://localhost:8080

#include <boost/asio.hpp>
#include <iostream>
#include <string>

// 简化命名空间
using boost::asio::ip::tcp;

// 构造 HTTP 响应
// 参数: body - JSON 响应体
// 返回: 完整 HTTP 响应字符串
std::string make_response(const std::string& body) {
    return "HTTP/1.1 200 OK\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: " + std::to_string(body.length()) + "\r\n"
           "\r\n" + body;
}

int main() {
    try {
        // io_context: Asio 的核心，管理所有 I/O 操作
        boost::asio::io_context io;
        
        // acceptor: 监听指定端口的连接
        // tcp::v4(): IPv4 协议
        // 8080: 监听端口
        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 8080));
        
        std::cout << "========================================" << std::endl;
        std::cout << "  NuClaw Step 0 - HTTP Echo Server" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Server listening on http://localhost:8080" << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        std::cout << std::endl;
        
        // 主循环：持续接受连接
        while (true) {
            // socket: 代表一个 TCP 连接
            tcp::socket socket(io);
            
            // accept(): 阻塞等待客户端连接
            // 有连接时，socket 会被赋值
            acceptor.accept(socket);
            
            std::cout << "[+] New connection accepted" << std::endl;
            
            // 读取请求（简化版，只读取前 1024 字节）
            // 实际 HTTP 请求可能更长，这里为了代码简洁做简化
            char buffer[1024] = {};
            size_t len = socket.read_some(boost::asio::buffer(buffer));
            
            std::cout << "[+] Received " << len << " bytes" << std::endl;
            
            // 构造 JSON 响应体
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
            // 这是短连接模式，每个请求后关闭
        }
        
    } catch (std::exception& e) {
        // 异常处理：打印错误信息
        std::cerr << "[!] Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
