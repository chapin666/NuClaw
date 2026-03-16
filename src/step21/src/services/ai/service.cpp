#include "smartsupport/services/ai/service.hpp"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace smartsupport::services::ai {

// TenantAgent 实现
TenantAgent::TenantAgent(const std::string& tenant_id,
                         const AgentConfig& config,
                         std::shared_ptr<knowledge::KnowledgeService> kb)
    : tenant_id_(tenant_id), config_(config), kb_(kb) {}

void TenantAgent::process(const std::string& session_id,
                          const std::vector<ChatMessage>& history,
                          const std::string& user_input,
                          std::function<void(Response)> callback) {
    
    // 1. RAG 检索相关知识
    std::vector<json> knowledge;
    if (config_.enable_rag) {
        knowledge = kb_>retrieve(tenant_id_, user_input, 5);
    }
    
    // 2. 构建 Prompt
    std::string prompt = build_prompt(history, user_input, knowledge);
    
    // 3. 模拟 LLM 调用（实际实现需要调用 OpenAI API）
    std::string llm_response = "This is a placeholder response for: " + user_input;
    
    // 4. 评估置信度
    float confidence = evaluate_confidence(llm_response, knowledge);
    
    // 5. 检查是否需要转人工
    bool needs_escalation = confidence < config_.confidence_threshold;
    
    // 6. 提取知识来源
    std::vector<std::string> sources;
    for (const auto& doc : knowledge) {
        if (doc.contains("title")) {
            sources.push_back(doc["title"]);
        }
    }
    
    callback(Response{
        .content = llm_response,
        .needs_escalation = needs_escalation,
        .confidence = confidence,
        .knowledge_sources = sources
    });
}

std::string TenantAgent::build_prompt(const std::vector<ChatMessage>& history,
                                      const std::string& user_input,
                                      const std::vector<json>& knowledge) {
    std::ostringstream prompt;
    
    prompt << config_.system_prompt << "\n\n";
    
    if (!knowledge.empty()) {
        prompt << "以下是相关知识，请基于这些信息回答：\n";
        for (size_t i = 0; i < knowledge.size(); ++i) {
            if (knowledge[i].contains("content")) {
                prompt << "[" << (i + 1) << "] " 
                          << knowledge[i]["content"].get<std::string>() << "\n";
            }
        }
        prompt << "\n";
    }
    
    size_t start = history.size() > 10 ? history.size() - 10 : 0;
    for (size_t i = start; i < history.size(); ++i) {
        prompt << (history[i].sender_type == "user" ? "用户" : "助手")
                  << "：" << history[i].content << "\n";
    }
    
    prompt << "用户：" << user_input << "\n助手：";
    
    return prompt.str();
}

float TenantAgent::evaluate_confidence(const std::string& response,
                                       const std::vector<json>& knowledge) {
    // 简化版置信度评估
    if (knowledge.empty()) return 0.5f;
    
    // 检查回答是否引用了知识
    float score = 0.6f;
    for (const auto& doc : knowledge) {
        if (doc.contains("content")) {
            std::string content = doc["content"].get<std::string>();
            // 简单检查回答中是否包含知识片段的关键词
            if (response.find(content.substr(0, 10)) != std::string::npos) {
                score += 0.1f;
            }
        }
    }
    
    return std::min(score, 1.0f);
}

// AIService 实现
AIService::AIService(asio::io_context& io, const Config& config,
                     std::shared_ptr<knowledge::KnowledgeService> kb_service)
    : io_(io), config_(config), kb_service_(kb_service) {}

TenantAgent& AIService::get_agent(const std::string& tenant_id) {
    std::lock_guard<std::mutex> lock(agents_mutex_);
    
    auto it = agents_.find(tenant_id);
    if (it == agents_.end()) {
        auto config = load_tenant_config(tenant_id);
        agents_[tenant_id] = std::make_unique<TenantAgent>(
            tenant_id, config, kb_service_);
        it = agents_.find(tenant_id);
    }
    
    return *it->second;
}

void AIService::process(const std::string& tenant_id,
                        const std::string& session_id,
                        const std::vector<ChatMessage>& history,
                        const std::string& user_input,
                        std::function<void(AIResponse)> callback) {
    
    auto& agent = get_agent(tenant_id);
    
    agent.process(session_id, history, user_input,
        [callback](const TenantAgent::Response& resp) {
            AIResponse ai_resp;
            ai_resp.content = resp.content;
            ai_resp.needs_escalation = resp.needs_escalation;
            ai_resp.confidence = resp.confidence;
            ai_resp.knowledge_sources = resp.knowledge_sources;
            callback(ai_resp);
        });
}

AgentConfig AIService::load_tenant_config(const std::string& tenant_id) {
    // 从数据库或配置服务加载租户配置
    AgentConfig config;
    config.system_prompt = "You are a helpful customer support agent.";
    config.temperature = 0.7f;
    config.enable_rag = true;
    config.confidence_threshold = 0.6f;
    return config;
}

} // namespace smartsupport::services::ai
