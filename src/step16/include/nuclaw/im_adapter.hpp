// im_adapter.hpp - IM 平台适配器接口
#pragma once
#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>

namespace nuclaw {

// 统一的消息格式
struct IMMessage {
    std::string platform;      // "feishu", "dingtalk", "telegram"
    std::string msg_id;
    std::string chat_id;       // 群聊/私聊 ID
    std::string user_id;
    std::string user_name;
    std::string content;       // 纯文本内容
    std::string raw_content;   // 原始富文本
    bool is_group = false;
    bool is_mentioned = false; // 是否被 @
    std::string msg_type;      // "text", "image", "file"
    nlohmann::json raw_data;   // 原始平台数据
};

// 发送消息请求
struct IMResponse {
    std::string chat_id;
    std::string content;
    std::string msg_type = "text";
    nlohmann::json rich_content; // 富文本/卡片消息
};

// IM 适配器接口
class IMAdapter {
public:
    virtual ~IMAdapter() = default;
    
    // 平台名称
    virtual std::string platform_name() const = 0;
    
    // 初始化（验证 token 等）
    virtual bool initialize(const nlohmann::json& config) = 0;
    
    // 解析 Webhook 请求
    virtual IMMessage parse_webhook(const nlohmann::json& request) = 0;
    
    // 发送消息
    virtual bool send_message(const IMResponse& response) = 0;
    
    // 验证签名（防篡改）
    virtual bool verify_signature(const std::string& signature, 
                                   const std::string& timestamp,
                                   const std::string& body) = 0;
};

// 适配器工厂
class AdapterFactory {
public:
    using Creator = std::function<std::unique_ptr<IMAdapter>()>;
    
    static AdapterFactory& instance();
    
    void register_adapter(const std::string& platform, Creator creator);
    std::unique_ptr<IMAdapter> create(const std::string& platform);
    std::vector<std::string> available_adapters() const;
    
private:
    std::map<std::string, Creator> creators_;
};

// 自动注册宏
#define REGISTER_ADAPTER(platform, class_name) \
    static struct class_name##Registrar { \
        class_name##Registrar() { \
            AdapterFactory::instance().register_adapter( \
                platform, []() { return std::make_unique<class_name>(); }); \
        } \
    } class_name## registrar;

} // namespace nuclaw
