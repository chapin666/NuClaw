// ============================================================================
// http_server.hpp - REST API HTTP 服务器（Boost.Beast 实现）
// ============================================================================
// 功能：提供 REST API 端点，替代 WebSocket 用于简单场景
//
// 示例：
//   HttpServer server(io, 8080);
//   server.post("/chat", [](const json::value& req) {
//       auto msg = req.at("message").as_string();
//       return json::object({{"reply", "收到: " + std::string(msg)}});
//   });
//   server.start();
// ============================================================================

#pragma once
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <functional>
#include <map>
#include <string>
#include <iostream>

namespace nuclaw {
namespace http {

namespace beast = boost::beast;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;
namespace json = boost::json;

// 请求处理器类型
using Handler = std::function<json::value(const json::value&)>;

// HTTP Server
class Server {
public:
    Server(asio::io_context& io, unsigned short port)
        : io_(io)
        , acceptor_(io, {tcp::v4(), port})
    {}
    
    // 注册路由
    void get(const std::string& path, Handler handler) {
        routes_["GET " + path] = handler;
    }
    
    void post(const std::string& path, Handler handler) {
        routes_["POST " + path] = handler;
    }
    
    // 启动服务器（非阻塞）
    void start() {
        do_accept();
        std::cout << "[HTTP Server] Started on port " 
                  << acceptor_.local_endpoint().port() << "\n";
    }
    
    // 停止服务器
    void stop() {
        acceptor_.close();
    }

private:
    asio::io_context& io_;
    tcp::acceptor acceptor_;
    std::map<std::string, Handler> routes_;
    beast::flat_buffer buffer_;  // 用于读取 HTTP 请求
    
    void do_accept();
    void handle_session(tcp::socket socket);
    void process_request(beast::http::request<beast::http::string_body>& req,
                         beast::http::response<beast::http::string_body>& resp);
};

// 接受连接
inline void Server::do_accept() {
    acceptor_.async_accept(
        [this](beast::error_code ec, tcp::socket socket) {
            if (!ec) {
                handle_session(std::move(socket));
            }
            do_accept();
        });
}

// 处理会话
inline void Server::handle_session(tcp::socket socket) {
    auto req = std::make_shared<beast::http::request<beast::http::string_body>>();
    
    beast::http::async_read(
        socket,
        buffer_,
        *req,
        [this, &socket, req](beast::error_code ec, std::size_t) {
            if (!ec) {
                beast::http::response<beast::http::string_body> resp;
                process_request(*req, resp);
                
                beast::http::write(socket, resp);
            }
            socket.shutdown(tcp::socket::shutdown_send, ec);
        });
}

// 处理请求
inline void Server::process_request(
    beast::http::request<beast::http::string_body>& req,
    beast::http::response<beast::http::string_body>& resp) {
    
    std::string route_key = std::string(req.method_string()) + " " + std::string(req.target());
    
    // 查找路由
    auto it = routes_.find(route_key);
    if (it != routes_.end()) {
        try {
            // 解析 JSON body
            json::value body = json::parse(req.body());
            
            // 调用处理器
            json::value result = it->second(body);
            
            // 设置成功响应
            resp.result(beast::http::status::ok);
            resp.set(beast::http::field::content_type, "application/json");
            resp.body() = json::serialize(result);
            
        } catch (const std::exception& e) {
            resp.result(beast::http::status::bad_request);
            resp.set(beast::http::field::content_type, "application/json");
            json::object error;
            error["error"] = e.what();
            resp.body() = json::serialize(error);
        }
    } else {
        // 404
        resp.result(beast::http::status::not_found);
        resp.set(beast::http::field::content_type, "application/json");
        json::object error;
        error["error"] = "Not found";
        error["route"] = route_key;
        resp.body() = json::serialize(error);
    }
    
    resp.prepare_payload();
}

} // namespace http
} // namespace nuclaw
