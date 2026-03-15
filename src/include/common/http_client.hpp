// ============================================================================
// http_client.hpp - 通用 HTTP 客户端（Boost.Beast 实现）
// ============================================================================
// 用途：为 LLM API、数据库等提供 HTTP 请求能力
// 依赖：Boost.Beast (header-only)
//
// 示例：
//   HttpClient client;
//   auto resp = client.post("https://api.openai.com/v1/chat/completions",
//                          headers, body, 30s);
// ============================================================================

#pragma once
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <string>
#include <map>
#include <chrono>

namespace nuclaw {
namespace http {

namespace beast = boost::beast;
namespace asio = boost::asio;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

// HTTP 响应结构
struct Response {
    int status_code = 0;
    std::map<std::string, std::string> headers;
    std::string body;
    bool success = false;
    std::string error;
    
    // 便捷判断
    bool ok() const { return status_code >= 200 && status_code < 300; }
};

// HTTP 客户端
class Client {
public:
    using Headers = std::map<std::string, std::string>;
    using Duration = std::chrono::seconds;
    
    Client() : ctx_(asio::ssl::context::tlsv12_client) {
        ctx_.set_default_verify_paths();
    }
    
    // GET 请求
    Response get(const std::string& url, 
                 const Headers& headers = {},
                 Duration timeout = std::chrono::seconds(30));
    
    // POST 请求
    Response post(const std::string& url,
                  const Headers& headers,
                  const std::string& body,
                  Duration timeout = std::chrono::seconds(30));
    
    // DELETE 请求
    Response del(const std::string& url,
                 const Headers& headers = {},
                 Duration timeout = std::chrono::seconds(30));

private:
    asio::ssl::context ctx_;
    
    // 解析 URL
    struct UrlParts {
        std::string protocol;
        std::string host;
        std::string port;
        std::string path;
    };
    UrlParts parse_url(const std::string& url);
    
    // 执行请求
    Response do_request(const std::string& method,
                        const UrlParts& url,
                        const Headers& headers,
                        const std::string& body,
                        Duration timeout);
};

// 同步 GET 实现（简化版）
inline Response Client::get(const std::string& url, 
                            const Headers& headers,
                            Duration timeout) {
    return do_request("GET", parse_url(url), headers, "", timeout);
}

// 同步 POST 实现
inline Response Client::post(const std::string& url,
                             const Headers& headers,
                             const std::string& body,
                             Duration timeout) {
    return do_request("POST", parse_url(url), headers, body, timeout);
}

// URL 解析
inline Client::UrlParts Client::parse_url(const std::string& url) {
    UrlParts parts;
    
    size_t protocol_end = url.find("://");
    if (protocol_end != std::string::npos) {
        parts.protocol = url.substr(0, protocol_end);
        protocol_end += 3;
    } else {
        protocol_end = 0;
    }
    
    size_t path_start = url.find('/', protocol_end);
    std::string host_port = (path_start == std::string::npos) 
        ? url.substr(protocol_end)
        : url.substr(protocol_end, path_start - protocol_end);
    
    size_t port_sep = host_port.find(':');
    if (port_sep != std::string::npos) {
        parts.host = host_port.substr(0, port_sep);
        parts.port = host_port.substr(port_sep + 1);
    } else {
        parts.host = host_port;
        parts.port = (parts.protocol == "https") ? "443" : "80";
    }
    
    if (path_start == std::string::npos) {
        parts.path = "/";
    } else {
        parts.path = url.substr(path_start);
    }
    
    return parts;
}

// 执行 HTTP 请求（简化同步实现）
inline Response Client::do_request(const std::string& method,
                                    const UrlParts& url,
                                    const Headers& headers,
                                    const std::string& body,
                                    Duration timeout) {
    Response resp;
    
    try {
        asio::io_context ioc;
        
        // 解析主机
        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve(url.host, url.port);
        
        // 创建连接
        beast::tcp_stream stream(ioc);
        stream.connect(results);
        stream.expires_after(timeout);
        
        // 构建请求
        beast::http::request<beast::http::string_body> req;
        req.method_string(method);
        req.target(url.path);
        req.version(11);
        req.set(beast::http::field::host, url.host);
        req.set(beast::http::field::user_agent, "NuClaw-HTTP-Client/1.0");
        
        // 添加自定义 headers
        for (const auto& [k, v] : headers) {
            req.set(k, v);
        }
        
        // 添加 body
        if (!body.empty()) {
            req.body() = body;
            req.prepare_payload();
        }
        
        // 发送请求
        beast::http::write(stream, req);
        
        // 读取响应
        beast::flat_buffer buffer;
        beast::http::response<beast::http::string_body> res;
        beast::http::read(stream, buffer, res);
        
        // 填充 Response
        resp.status_code = res.result_int();
        resp.body = res.body();
        resp.success = resp.ok();
        
        for (const auto& field : res.base()) {
            resp.headers[std::string(field.name_string())] = std::string(field.value());
        }
        
        // 关闭连接
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
        
    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = e.what();
    }
    
    return resp;
}

} // namespace http
} // namespace nuclaw
