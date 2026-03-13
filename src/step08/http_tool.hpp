// ============================================================================
// http_tool.hpp - HTTP GET 工具
// ============================================================================

#pragma once

#include "tool.hpp"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <utility>

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class HttpGetTool : public Tool {
public:
    std::string get_name() const override { return "http_get"; }
    std::string get_description() const override { return "Send HTTP GET request (with SSRF protection)"; }
    
    json::value execute(const json::object& args) override {
        std::string url(args.at("url").as_string());
        
        if (!is_allowed_url(url)) {
            json::object error;
            error["success"] = false;
            error["error"] = "URL not in whitelist (SSRF protection)";
            return error;
        }
        
        try {
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
            result["body"] = beast::buffers_to_string(res.body().data()).substr(0, 5000);
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
        if (url.find("http://") != 0) return false;
        if (url.find("localhost") != std::string::npos) return false;
        if (url.find("127.0.") != std::string::npos) return false;
        if (url.find("192.168.") != std::string::npos) return false;
        if (url.find("10.") != std::string::npos) return false;
        return true;
    }
    
    std::pair<std::string, std::string> parse_url(const std::string& url) {
        size_t host_start = url.find("://") + 3;
        size_t path_start = url.find('/', host_start);
        
        std::string host = url.substr(host_start, path_start - host_start);
        std::string target = (path_start == std::string::npos) 
            ? "/" : url.substr(path_start);
        
        return {host, target};
    }
};
