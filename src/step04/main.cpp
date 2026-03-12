// src/step04/main.cpp
// Step 4: LLM Integration (OpenAI)
// 编译: g++ -std=c++17 main.cpp -o server -lboost_system -lssl -lcrypto -lpthread

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <memory>

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
namespace json = boost::json;
using tcp = asio::ip::tcp;
namespace ssl = asio::ssl;

// LLM 客户端
class LLMClient {
public:
    LLMClient(const std::string& api_key) : api_key_(api_key) {}
    
    std::string chat(const std::vector<std::pair<std::string, std::string>>& messages) {
        try {
            asio::io_context io;
            ssl::context ctx(ssl::context::tlsv12_client);
            ctx.set_default_verify_paths();
            
            tcp::resolver resolver(io);
            beast::ssl_stream<beast::tcp_stream> stream(io, ctx);
            
            if (!SSL_set_tlsext_host_name(stream.native_handle(), "api.openai.com")) {
                return "Error: SSL SNI failed";
            }
            
            auto results = resolver.resolve("api.openai.com", "443");
            beast::get_lowest_layer(stream).connect(results);
            stream.handshake(ssl::stream_base::client);
            
            // 构建请求体
            json::object req_body;
            req_body["model"] = "gpt-3.5-turbo";
            json::array msgs;
            for (const auto& [role, content] : messages) {
                json::object msg;
                msg["role"] = role;
                msg["content"] = content;
                msgs.push_back(msg);
            }
            req_body["messages"] = msgs;
            req_body["temperature"] = 0.7;
            
            std::string body = json::serialize(req_body);
            
            // HTTP 请求
            http::request<http::string_body> req{http::verb::post, "/v1/chat/completions", 11};
            req.set(http::field::host, "api.openai.com");
            req.set(http::field::authorization, "Bearer " + api_key_);
            req.set(http::field::content_type, "application/json");
            req.content_length(body.size());
            req.body() = body;
            
            http::write(stream, req);
            
            beast::flat_buffer buffer;
            http::response<http::string_body> res;
            http::read(stream, buffer, res);
            
            beast::error_code ec;
            stream.shutdown(ec);
            
            if (res.result() == http::status::ok) {
                auto j = json::parse(res.body());
                auto& choices = j.at("choices").as_array();
                if (!choices.empty()) {
                    return std::string(choices[0].at("message").at("content").as_string());
                }
            }
            return "Error: " + res.body();
            
        } catch (std::exception& e) {
            return std::string("Exception: ") + e.what();
        }
    }
    
private:
    std::string api_key_;
};

// Agent with LLM
class Agent {
public:
    Agent(const std::string& api_key) : llm_(api_key), use_llm_(!api_key.empty()) {}
    
    std::string process(const std::string& input) {
        // 内置命令
        if (input.find("/echo ") == 0) {
            return input.substr(6);
        }
        if (input == "/clear") {
            history_.clear();
            return "History cleared.";
        }
        if (input == "/llm_off") {
            use_llm_ = false;
            return "LLM disabled.";
        }
        if (input == "/llm_on") {
            use_llm_ = true;
            return "LLM enabled.";
        }
        
        // 正常对话
        history_.push_back({"user", input});
        
        std::string response;
        if (use_llm_) {
            response = llm_.chat(history_);
        } else {
            response = "Echo: " + input;
        }
        
        history_.push_back({"assistant", response});
        return response;
    }
    
private:
    LLMClient llm_;
    std::vector<std::pair<std::string, std::string>> history_;
    bool use_llm_;
};

// 简化 WebSocket Server
#include <boost/beast/websocket.hpp>

class WsServer {
public:
    WsServer(asio::io_context& io, short port, const std::string& api_key)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)), agent_(api_key) {
        do_accept();
    }
    
private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<Session>(std::move(socket), agent_)->start();
                }
                do_accept();
            });
    }
    
    class Session : public std::enable_shared_from_this<Session> {
    public:
        Session(tcp::socket socket, Agent& agent)
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
                        [self](beast::error_code ec, std::size_t) {
                            if (!ec) self->do_read();
                        });
                });
        }
        
        beast::websocket::stream<tcp::socket> ws_;
        Agent& agent_;
        beast::flat_buffer buffer_;
    };
    
    tcp::acceptor acceptor_;
    Agent agent_;
};

int main() {
    const char* api_key = std::getenv("OPENAI_API_KEY");
    
    std::cout << "NuClaw Step 4 - LLM Integration\n";
    if (!api_key) {
        std::cout << "[DEMO MODE] Set OPENAI_API_KEY for real LLM calls\n";
    }
    
    try {
        asio::io_context io;
        
        WsServer server(io, 8081, api_key ? api_key : "");
        
        std::cout << "WebSocket: ws://localhost:8081\n";
        std::cout << "Commands: /echo, /clear, /llm_on, /llm_off\n";
        
        io.run();
        
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}
