// ============================================================================
// llm_http_client.hpp - 真实 LLM HTTP 客户端
// ============================================================================
// 功能：调用 OpenAI / Moonshot / Claude 等真实 LLM API
// 
// 环境变量：
//   OPENAI_API_KEY    - OpenAI API Key
//   MOONSHOT_API_KEY  - Moonshot API Key  
//   LLM_PROVIDER      - openai / moonshot / claude (默认: openai)
//   LLM_MODEL         - 模型名称 (默认: gpt-3.5-turbo)
//
// 示例：
//   LLMHttpClient llm;
//   auto resp = llm.chat("你好");
//   std::cout << resp.content;
// ============================================================================

#pragma once
#include "http_client.hpp"
#include <boost/json.hpp>
#include <cstdlib>
#include <iostream>

namespace nuclaw {

namespace json = boost::json;

// LLM 响应结构
struct LLMResponse {
    bool success = false;
    std::string content;
    std::string error;
    int status_code = 0;
    int tokens_used = 0;
    std::string model;
    double latency_ms = 0;
};

// LLM HTTP 客户端
class LLMHttpClient {
public:
    struct Config {
        std::string provider;   // openai, moonshot, claude
        std::string api_key;
        std::string model;
        std::string base_url;
        float temperature = 0.7f;
        int max_tokens = 1000;
    };
    
    // 从环境变量加载配置
    LLMHttpClient() : http_client_(std::make_unique<http::Client>()) {
        load_config_from_env();
    }
    
    // 显式配置
    explicit LLMHttpClient(const Config& config) 
        : config_(config), http_client_(std::make_unique<http::Client>()) {}
    
    // 简单对话
    LLMResponse chat(const std::string& message);
    
    // 带历史记录的对话
    LLMResponse chat_with_history(const std::vector<std::pair<std::string, std::string>>& history,
                                   const std::string& message);
    
    // 检查是否已配置
    bool is_configured() const { return !config_.api_key.empty(); }
    
    // 获取配置
    const Config& config() const { return config_; }

private:
    Config config_;
    std::unique_ptr<http::Client> http_client_;
    
    void load_config_from_env();
    std::string build_request_body(const std::vector<std::pair<std::string, std::string>>& messages);
    LLMResponse parse_response(const http::Response& http_resp);
};

// 从环境变量加载配置
inline void LLMHttpClient::load_config_from_env() {
    const char* provider = std::getenv("LLM_PROVIDER");
    config_.provider = provider ? provider : "openai";
    
    if (config_.provider == "openai") {
        const char* key = std::getenv("OPENAI_API_KEY");
        config_.api_key = key ? key : "";
        config_.base_url = "https://api.openai.com/v1/chat/completions";
        
        const char* model = std::getenv("LLM_MODEL");
        config_.model = model ? model : "gpt-3.5-turbo";
    }
    else if (config_.provider == "moonshot") {
        const char* key = std::getenv("MOONSHOT_API_KEY");
        config_.api_key = key ? key : "";
        config_.base_url = "https://api.moonshot.cn/v1/chat/completions";
        config_.model = "moonshot-v1-8k";
    }
}

// 简单对话
inline LLMResponse LLMHttpClient::chat(const std::string& message) {
    std::vector<std::pair<std::string, std::string>> messages;
    messages.push_back({"user", message});
    return chat_with_history({}, message);
}

// 带历史记录的对话
inline LLMResponse LLMHttpClient::chat_with_history(
    const std::vector<std::pair<std::string, std::string>>& history,
    const std::string& message) {
    
    LLMResponse result;
    
    if (!is_configured()) {
        result.error = "LLM not configured. Please set OPENAI_API_KEY or MOONSHOT_API_KEY";
        return result;
    }
    
    auto start = std::chrono::steady_clock::now();
    
    // 构建消息列表
    std::vector<std::pair<std::string, std::string>> messages = history;
    messages.push_back({"user", message});
    
    // 构建请求 body
    std::string body = build_request_body(messages);
    
    // 设置 headers
    http::Client::Headers headers;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = "Bearer " + config_.api_key;
    
    // 发送请求
    auto http_resp = http_client_->post(config_.base_url, headers, body, std::chrono::seconds(60));
    
    // 计算延迟
    auto end = std::chrono::steady_clock::now();
    result.latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // 解析响应
    if (http_resp.success) {
        return parse_response(http_resp);
    } else {
        result.error = http_resp.error.empty() 
            ? "HTTP " + std::to_string(http_resp.status_code)
            : http_resp.error;
        return result;
    }
}

// 构建请求 body
inline std::string LLMHttpClient::build_request_body(
    const std::vector<std::pair<std::string, std::string>>& messages) {
    
    json::array msgs;
    for (const auto& [role, content] : messages) {
        json::object msg;
        msg["role"] = role;
        msg["content"] = content;
        msgs.push_back(std::move(msg));
    }
    
    json::object req;
    req["model"] = config_.model;
    req["messages"] = std::move(msgs);
    req["temperature"] = config_.temperature;
    req["max_tokens"] = config_.max_tokens;
    
    return json::serialize(req);
}

// 解析响应
inline LLMResponse LLMHttpClient::parse_response(const http::Response& http_resp) {
    LLMResponse result;
    result.status_code = http_resp.status_code;
    
    try {
        json::value data = json::parse(http_resp.body);
        
        // 检查错误
        if (data.as_object().contains("error")) {
            auto& err = data.at("error").as_object();
            result.error = std::string(err.at("message").as_string());
            return result;
        }
        
        // 提取内容
        auto& choices = data.at("choices").as_array();
        if (!choices.empty()) {
            auto& message = choices[0].at("message").as_object();
            result.content = std::string(message.at("content").as_string());
            result.success = true;
        }
        
        // 提取 token 使用量
        if (data.as_object().contains("usage")) {
            auto& usage = data.at("usage").as_object();
            if (usage.contains("total_tokens")) {
                result.tokens_used = static_cast<int>(usage.at("total_tokens").as_int64());
            }
        }
        
        // 提取模型
        if (data.as_object().contains("model")) {
            result.model = std::string(data.at("model").as_string());
        }
        
    } catch (const std::exception& e) {
        result.error = std::string("Parse error: ") + e.what();
    }
    
    return result;
}

} // namespace nuclaw
