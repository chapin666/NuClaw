// dingtalk_adapter.hpp - 钉钉适配器
#pragma once
#include "im_adapter.hpp"
#include <cpp-httplib/httplib.h>

namespace nuclaw {

class DingTalkAdapter : public IMAdapter {
public:
    std::string platform_name() const override { return "dingtalk"; }
    
    bool initialize(const nlohmann::json& config) override {
        app_key_ = config.value("app_key", "");
        app_secret_ = config.value("app_secret", "");
        webhook_token_ = config.value("webhook_token", "");
        
        return refresh_access_token();
    }
    
    IMMessage parse_webhook(const nlohmann::json& req) override {
        IMMessage msg;
        msg.platform = "dingtalk";
        msg.raw_data = req;
        
        // 解析钉钉消息格式
        auto content = req.value("text", nlohmann::json{});
        msg.content = content.value("content", "");
        msg.raw_content = msg.content;
        
        msg.msg_id = req.value("msgId", "");
        msg.chat_id = req.value("conversationId", "");
        msg.user_id = req.value("senderStaffId", "");
        msg.user_name = req.value("senderNick", "");
        
        // 检查是否群聊
        msg.is_group = req.value("conversationType", "1") == "2";
        
        // 检查是否被 @（钉钉通过 atUsers 字段）
        if (req.contains("atUsers")) {
            for (const auto& at : req["atUsers"]) {
                if (at.value("staffId", "") == "your-bot-staff-id") {
                    msg.is_mentioned = true;
                    break;
                }
            }
        }
        
        // 移除 @机器人的前缀
        if (msg.is_mentioned && !msg.content.empty()) {
            // 钉钉格式：@机器人 消息内容
            size_t pos = msg.content.find(' ');
            if (pos != std::string::npos) {
                msg.content = msg.content.substr(pos + 1);
            }
        }
        
        return msg;
    }
    
    bool send_message(const IMResponse& response) override {
        httplib::Client cli("https://oapi.dingtalk.com");
        
        nlohmann::json body = {
            {"chatid", response.chat_id},
            {"msg", {
                {"msgtype", "text"},
                {"text", {{"content", response.content}}}
            }}
        };
        
        auto url = fmt::format("/topapi/message/corpconversation/asyncsend_v2?access_token={}", 
                              access_token_);
        
        auto res = cli.Post(url.c_str(), body.dump(), "application/json");
        
        return res && res->status == 200;
    }
    
    bool verify_signature(const std::string& signature,
                         const std::string& timestamp,
                         const std::string& body) override {
        // 钉钉签名验证
        // 1. 把 timestamp + "\n" + 密钥 作为签名字符串
        // 2. 使用 HmacSHA256 计算签名
        // 3. 进行 Base64 encode
        // 简化实现
        return true;
    }

private:
    bool refresh_access_token() {
        httplib::Client cli("https://oapi.dingtalk.com");
        
        auto url = fmt::format("/gettoken?appkey={}&appsecret={}", 
                              app_key_, app_secret_);
        
        auto res = cli.Get(url.c_str());
        
        if (res && res->status == 200) {
            auto resp = nlohmann::json::parse(res->body);
            access_token_ = resp.value("access_token", "");
            return !access_token_.empty();
        }
        return false;
    }
    
    std::string app_key_;
    std::string app_secret_;
    std::string webhook_token_;
    std::string access_token_;
};

REGISTER_ADAPTER("dingtalk", DingTalkAdapter);

} // namespace nuclaw
