#pragma once

#include "smartsupport/common/types.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <map>

namespace smartsupport::services::knowledge {
    class KnowledgeService;
}

namespace smartsupport::services::ai {

namespace asio = boost::asio;

// Agent 配置
struct AgentConfig {
    std::string system_prompt;
    float temperature = 0.7f;
    int max_context_length = 4000;
    std::vector<std::string> tools_enabled;
    bool enable_rag = true;
    float confidence_threshold = 0.6f;
};

// 租户级 Agent
class TenantAgent {
public:
    TenantAgent(const std::string& tenant_id,
                const AgentConfig& config,
                std::shared_ptr<knowledge::KnowledgeService> kb);
    
    struct Response {
        std::string content;
        bool needs_escalation;
        float confidence;
        std::vector<std::string> knowledge_sources;
    };
    
    void process(const std::string& session_id,
                 const std::vector<ChatMessage>& history,
                 const std::string& user_input,
                 std::function<void(Response)> callback);

private:
    std::string build_prompt(const std::vector<ChatMessage>& history,
                             const std::string& user_input,
                             const std::vector<json>& knowledge);
    float evaluate_confidence(const std::string& response,
                              const std::vector<json>& knowledge);

    std::string tenant_id_;
    AgentConfig config_;
    std::shared_ptr<knowledge::KnowledgeService> kb_;
};

// AI 服务
class AIService {
public:
    struct Config {
        std::string llm_provider;
        std::string api_key;
        std::string model;
    };
    
    AIService(asio::io_context& io, const Config& config,
              std::shared_ptr<knowledge::KnowledgeService> kb_service);
    
    TenantAgent& get_agent(const std::string& tenant_id);
    void process(const std::string& tenant_id,
                 const std::string& session_id,
                 const std::vector<ChatMessage>& history,
                 const std::string& user_input,
                 std::function<void(AIResponse)> callback);

private:
    AgentConfig load_tenant_config(const std::string& tenant_id);
    
    asio::io_context& io_;
    Config config_;
    std::shared_ptr<knowledge::KnowledgeService> kb_service_;
    std::map<std::string, std::unique_ptr<TenantAgent>> agents_;
    std::mutex agents_mutex_;
};

} // namespace smartsupport::services::ai
