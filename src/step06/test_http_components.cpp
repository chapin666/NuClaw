// ============================================================================
// test_http_components.cpp - 验证 HTTP 组件
// ============================================================================

#include "common/http_client.hpp"
#include "common/llm_http_client.hpp"
#include <iostream>
#include <cstdlib>

using namespace nuclaw;

int main() {
    std::cout << "========================================\n";
    std::cout << "HTTP 组件测试\n";
    std::cout << "========================================\n\n";
    
    // 测试 1：HTTP 客户端
    std::cout << "[测试 1] HTTP GET httpbin.org\n";
    {
        http::Client client;
        auto resp = client.get("http://httpbin.org/get", {}, std::chrono::seconds(10));
        std::cout << "  Status: " << resp.status_code << "\n";
        std::cout << "  Success: " << (resp.success ? "✅" : "❌") << "\n";
        std::cout << "  Body size: " << resp.body.size() << " bytes\n\n";
    }
    
    // 测试 2：LLM 客户端配置检查
    std::cout << "[测试 2] LLM 客户端配置\n";
    {
        LLMHttpClient llm;
        std::cout << "  Provider: " << llm.config().provider << "\n";
        std::cout << "  Model: " << llm.config().model << "\n";
        std::cout << "  API Key: " << (llm.is_configured() ? "✅ 已配置" : "❌ 未配置") << "\n";
        std::cout << "  Base URL: " << llm.config().base_url << "\n\n";
    }
    
    // 测试 3：真实 LLM 调用（如果配置了 API Key）
    if (std::getenv("OPENAI_API_KEY")) {
        std::cout << "[测试 3] 真实 LLM 调用\n";
        LLMHttpClient llm;
        auto resp = llm.chat("你好，请用一句话介绍自己");
        std::cout << "  Success: " << (resp.success ? "✅" : "❌") << "\n";
        if (resp.success) {
            std::cout << "  Content: " << resp.content << "\n";
            std::cout << "  Tokens: " << resp.tokens_used << "\n";
            std::cout << "  Latency: " << resp.latency_ms << " ms\n";
        } else {
            std::cout << "  Error: " << resp.error << "\n";
        }
    } else {
        std::cout << "[测试 3] 跳过 LLM 调用（未设置 OPENAI_API_KEY）\n";
    }
    
    std::cout << "\n========================================\n";
    std::cout << "测试完成\n";
    std::cout << "========================================\n";
    
    return 0;
}
