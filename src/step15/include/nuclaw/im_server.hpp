// im_server.hpp - IM 消息服务器
#pragma once
#include "im_adapter.hpp"
#include <nuclaw/chat_engine.hpp>
#include <cpp-httplib/httplib.h>
#include <thread>

namespace nuclaw {

class IMServer {
public:
    IMServer(std::shared_ptr<ChatEngine> engine, int port = 8080)
        : chat_engine_(engine), port_(port) {}
    
    // 注册 IM 平台适配器
    void register_adapter(const std::string& platform, 
                         std::unique_ptr<IMAdapter> adapter) {
        adapters_[platform] = std::move(adapter);
    }
    
    // 启动服务器
    void start() {
        using namespace httplib;
        
        // Webhook 接收端点
        server_.Post("/webhook/:platform", [this](const Request& req, Response& res) {
            auto platform = req.path_params.at("platform");
            
            auto it = adapters_.find(platform);
            if (it == adapters_.end()) {
                res.status = 404;
                res.set_content(R"({"error": "platform not supported"})", "application/json");
                return;
            }
            
            auto& adapter = it->second;
            
            // 验证签名（安全校验）
            auto signature = req.get_header_value("X-Hub-Signature");
            auto timestamp = req.get_header_value("X-Request-Timestamp");
            
            if (!adapter->verify_signature(signature, timestamp, req.body)) {
                res.status = 401;
                res.set_content(R"({"error": "invalid signature"})", "application/json");
                return;
            }
            
            try {
                // 解析消息
                auto json_req = nlohmann::json::parse(req.body);
                auto message = adapter->parse_webhook(json_req);
                
                // 处理消息
                auto response = handle_message(message);
                
                // 发送回复
                if (adapter->send_message(response)) {
                    res.set_content(R"({"status": "ok"})", "application/json");
                } else {
                    res.status = 500;
                    res.set_content(R"({"error": "send failed"})", "application/json");
                }
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(fmt::format(R"({{"error": "{}"}})", e.what()), 
                              "application/json");
            }
        });
        
        // 健康检查
        server_.Get("/health", [](const Request& req, Response& res) {
            res.set_content(R"({"status": "healthy"})", "application/json");
        });
        
        std::cout << "IM Server starting on port " << port_ << std::endl;
        server_.listen("0.0.0.0", port_);
    }
    
    void stop() {
        server_.stop();
    }

private:
    IMResponse handle_message(const IMMessage& msg) {
        // 构建上下文
        std::string context;
        if (msg.is_group) {
            context = fmt::format("[群聊消息] {} 说：{}", 
                                msg.user_name, msg.content);
        } else {
            context = fmt::format("[私聊] {} 说：{}", 
                                msg.user_name, msg.content);
        }
        
        // 调用 ChatEngine 处理
        auto reply = chat_engine_->process(context);
        
        return IMResponse{
            .chat_id = msg.chat_id,
            .content = reply,
            .msg_type = "text"
        };
    }
    
    std::shared_ptr<ChatEngine> chat_engine_;
    std::map<std::string, std::unique_ptr<IMAdapter>> adapters_;
    httplib::Server server_;
    int port_;
};

} // namespace nuclaw
