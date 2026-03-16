// feishu_adapter.hpp - 飞书适配器
#pragma once
#include "im_adapter.hpp"
#include <cpp-httplib/httplib.h>

namespace nuclaw {

class FeishuAdapter : public IMAdapter {
public:
    std::string platform_name() const override { return "feishu"; }
    
    bool initialize(const nlohmann::json& config) override {
        app_id_ = config.value("app_id", "");
        app_secret_ = config.value("app_secret", "");
        webhook_verify_token_ = config.value("webhook_verify_token", "");
        
        // 获取 tenant access token
        return refresh_token();
    }
    
    IMMessage parse_webhook(const nlohmann::json& req) override {
        IMMessage msg;
        msg.platform = "feishu";
        msg.raw_data = req;
        
        // 解析飞书消息格式
        auto event = req.value("event", nlohmann::json{});
        
        // 消息内容
        if (event.contains("message")) {
            auto message = event["message"];
            msg.msg_id = message.value("message_id", "");
            msg.chat_id = message.value("chat_id", "");
            msg.user_id = message.value("sender").value("sender_id", "");
            msg.msg_type = message.value("msg_type", "text");
            
            // 解析文本内容
            if (msg.msg_type == "text") {
                auto content = nlohmann::json::parse(message.value("content", "{}"));
                msg.content = content.value("text", "");
                msg.raw_content = msg.content;
            }
            
            // 检查是否群聊
            msg.is_group = message.value("chat_type", "p2p") == "group";
            
            // 检查是否被 @（通过 mentions 字段）
            if (message.contains("mentions")) {
                for (const auto& mention : message["mentions"]) {
                    if (mention.value("name", "") == "BotName") {
                        msg.is_mentioned = true;
                        break;
                    }
                }
            }
        }
        
        return msg;
    }
    
    bool send_message(const IMResponse& response) override {
        httplib::Client cli("https://open.feishu.cn");
        
        nlohmann::json body = {
            {"receive_id", response.chat_id},
            {"content", {{"text", response.content}}},
            {"msg_type", "text"}
        };
        
        httplib::Headers headers = {
            {"Authorization", "Bearer " + tenant_token_},
            {"Content-Type", "application/json"}
        };
        
        auto res = cli.Post("/open-apis/im/v1/messages", 
                           headers, body.dump(), "application/json");
        
        return res && res->status == 200;
    }
    
    bool verify_signature(const std::string& signature,
                         const std::string& timestamp,
                         const std::string& body) override {
        // 飞书签名验证：SHA256(timestamp + nonce + body + secret)
        // 简化实现，实际需完整实现
        return !webhook_verify_token_.empty();
    }

private:
    bool refresh_token() {
        httplib::Client cli("https://open.feishu.cn");
        
        nlohmann::json body = {
            {"app_id", app_id_},
            {"app_secret", app_secret_}
        };
        
        auto res = cli.Post("/open-apis/auth/v3/tenant_access_token/internal",
                           body.dump(), "application/json");
        
        if (res && res->status == 200) {
            auto resp = nlohmann::json::parse(res->body);
            tenant_token_ = resp.value("tenant_access_token", "");
            return !tenant_token_.empty();
        }
        return false;
    }
    
    std::string app_id_;
    std::string app_secret_;
    std::string webhook_verify_token_;
    std::string tenant_token_;
};

REGISTER_ADAPTER("feishu", FeishuAdapter);

} // namespace nuclaw
