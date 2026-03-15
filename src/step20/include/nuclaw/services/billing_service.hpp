// ============================================================================
// billing_service.hpp - Step 19: 计费服务
// ============================================================================
// 演进说明：
//   Step 19 新增，为 SaaS 平台提供按量计费能力
//
//   功能：
//     - 用量统计（消息数、Token 数、存储量）
//     - 实时计费
//     - 套餐配额检查
//     - 计费告警
// ============================================================================

#pragma once
#include <memory>
#include <map>
#include <mutex>
#include <chrono>

namespace nuclaw {

// 用量统计
struct UsageStats {
    std::string tenant_id;
    
    // 消息统计
    int total_messages = 0;           // 总消息数
    int ai_messages = 0;              // AI 回复数
    int human_messages = 0;           // 人工回复数
    
    // Token 统计（LLM 用量）
    int input_tokens = 0;             // 输入 Token 数
    int output_tokens = 0;            // 输出 Token 数
    
    // 存储统计
    int knowledge_documents = 0;      // 知识库文档数
    int storage_mb = 0;               // 存储空间（MB）
    
    // 会话统计
    int total_sessions = 0;           // 总会话数
    int active_sessions = 0;          // 当前活跃会话数
    int max_concurrent_sessions = 0;  // 峰值并发
    
    // 时间
    std::chrono::system_clock::time_point period_start;
    std::chrono::system_clock::time_point last_updated;
    
    UsageStats() {
        period_start = std::chrono::system_clock::now();
        last_updated = period_start;
    }
};

// 计费告警
struct BillingAlert {
    std::string alert_id;
    std::string tenant_id;
    std::string alert_type;     // "quota_warning", "quota_exceeded", "cost_threshold"
    std::string message;
    std::chrono::system_clock::time_point created_at;
    bool is_resolved;
};

// 计费配置
struct BillingConfig {
    // Token 单价（美元/1K tokens）
    double input_token_price = 0.0015;   // GPT-3.5 输入价格
    double output_token_price = 0.002;   // GPT-3.5 输出价格
    
    // 消息单价（美元/条）
    double ai_message_price = 0.01;
    double human_message_price = 0.05;   // 人工客服更贵
    
    // 存储单价（美元/GB/月）
    double storage_price = 0.1;
    
    // 告警阈值
    int quota_warning_threshold = 80;    // 配额使用 80% 时告警
    double cost_alert_threshold = 100.0; // 费用超过 $100 时告警
};

// BillingService: 计费管理（Step 19 新增）
class BillingService {
public:
    explicit BillingService(const BillingConfig& config = BillingConfig{})
        : config_(config) {}
    
    // ============================================================
    // 用量记录
    // ============================================================
    
    // 记录 AI 消息
    void record_ai_message(const std::string& tenant_id, 
                           int input_tokens, 
                           int output_tokens) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto& stats = get_or_create_stats_unlocked(tenant_id);
        stats.ai_messages++;
        stats.total_messages++;
        stats.input_tokens += input_tokens;
        stats.output_tokens += output_tokens;
        stats.last_updated = std::chrono::system_clock::now();
        
        // 检查配额
        check_quota_unlocked(tenant_id);
    }
    
    // 记录人工客服消息
    void record_human_message(const std::string& tenant_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto& stats = get_or_create_stats_unlocked(tenant_id);
        stats.human_messages++;
        stats.total_messages++;
        stats.last_updated = std::chrono::system_clock::now();
    }
    
    // 记录会话开始
    void record_session_start(const std::string& tenant_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto& stats = get_or_create_stats_unlocked(tenant_id);
        stats.total_sessions++;
        stats.active_sessions++;
        stats.max_concurrent_sessions = std::max(
            stats.max_concurrent_sessions, stats.active_sessions);
        stats.last_updated = std::chrono::system_clock::now();
    }
    
    // 记录会话结束
    void record_session_end(const std::string& tenant_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = stats_.find(tenant_id);
        if (it != stats_.end()) {
            it->second.active_sessions = std::max(0, it->second.active_sessions - 1);
            it->second.last_updated = std::chrono::system_clock::now();
        }
    }
    
    // 记录知识库文档
    void record_knowledge_document(const std::string& tenant_id, 
                                    int doc_size_mb = 1) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto& stats = get_or_create_stats_unlocked(tenant_id);
        stats.knowledge_documents++;
        stats.storage_mb += doc_size_mb;
        stats.last_updated = std::chrono::system_clock::now();
    }
    
    // ============================================================
    // 费用计算
    // ============================================================
    
    // 计算当前周期费用
    double calculate_cost(const std::string& tenant_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = stats_.find(tenant_id);
        if (it == stats_.end()) return 0.0;
        
        const auto& stats = it->second;
        
        double cost = 0.0;
        
        // Token 费用
        cost += (stats.input_tokens / 1000.0) * config_.input_token_price;
        cost += (stats.output_tokens / 1000.0) * config_.output_token_price;
        
        // 消息费用
        cost += stats.ai_messages * config_.ai_message_price;
        cost += stats.human_messages * config_.human_message_price;
        
        // 存储费用
        cost += (stats.storage_mb / 1024.0) * config_.storage_price;
        
        return cost;
    }
    
    // 获取用量统计
    UsageStats get_usage_stats(const std::string& tenant_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = stats_.find(tenant_id);
        if (it != stats_.end()) {
            return it->second;
        }
        return UsageStats{};
    }
    
    // ============================================================
    // 配额检查
    // ============================================================
    
    // 检查是否超出配额（供其他服务调用）
    struct QuotaCheck {
        bool allowed;
        std::string reason;  // 如果不允许，说明原因
    };
    
    QuotaCheck check_message_quota(const std::string& tenant_id, 
                                    int current_monthly_count,
                                    int max_monthly_count) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (current_monthly_count >= max_monthly_count) {
            return {false, "本月消息配额已用完"};
        }
        
        // 检查是否接近阈值
        double usage_percent = (double)current_monthly_count / max_monthly_count * 100;
        if (usage_percent >= config_.quota_warning_threshold) {
            create_alert_unlocked(tenant_id, "quota_warning", 
                "消息配额使用已达到 " + std::to_string((int)usage_percent) + "%");
        }
        
        return {true, ""};
    }
    
    QuotaCheck check_concurrent_sessions(const std::string& tenant_id,
                                          int current_active,
                                          int max_allowed) {
        if (current_active >= max_allowed) {
            return {false, "并发会话数已达上限"};
        }
        return {true, ""};
    }
    
    // ============================================================
    // 告警管理
    // ============================================================
    
    std::vector<BillingAlert> get_alerts(const std::string& tenant_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<BillingAlert> result;
        for (const auto& alert : alerts_) {
            if (alert.tenant_id == tenant_id && !alert.is_resolved) {
                result.push_back(alert);
            }
        }
        return result;
    }
    
    void resolve_alert(const std::string& alert_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& alert : alerts_) {
            if (alert.alert_id == alert_id) {
                alert.is_resolved = true;
                break;
            }
        }
    }
    
    // ============================================================
    // 统计和报告
    // ============================================================
    
    void print_stats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::cout << "\n[BillingService 统计]\n";
        std::cout << "  监控租户数: " << stats_.size() << "\n";
        
        double total_revenue = 0.0;
        int total_messages = 0;
        int total_tokens = 0;
        
        for (const auto& [tenant_id, stats] : stats_) {
            // 简化的费用计算
            double cost = (stats.input_tokens / 1000.0) * config_.input_token_price +
                         (stats.output_tokens / 1000.0) * config_.output_token_price +
                         stats.ai_messages * config_.ai_message_price +
                         stats.human_messages * config_.human_message_price;
            
            total_revenue += cost;
            total_messages += stats.total_messages;
            total_tokens += stats.input_tokens + stats.output_tokens;
        }
        
        std::cout << "  总消息数: " << total_messages << "\n";
        std::cout << "  总 Token: " << total_tokens << "\n";
        std::cout << "  预估收入: $" << total_revenue << "\n";
        std::cout << "  活跃告警: " << alerts_.size() << "\n";
    }
    
    // 生成租户用量报告
    void print_tenant_report(const std::string& tenant_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = stats_.find(tenant_id);
        if (it == stats_.end()) {
            std::cout << "租户 " << tenant_id << " 无用量数据\n";
            return;
        }
        
        const auto& s = it->second;
        double cost = calculate_cost_unlocked(s);
        
        std::cout << "\n[租户 " << tenant_id << " 用量报告]\n";
        std::cout << "  消息: AI=" << s.ai_messages << " 人工=" << s.human_messages << "\n";
        std::cout << "  Token: 输入=" << s.input_tokens << " 输出=" << s.output_tokens << "\n";
        std::cout << "  存储: " << s.knowledge_documents << " 文档, " << s.storage_mb << " MB\n";
        std::cout << "  会话: 总计=" << s.total_sessions << " 峰值并发=" << s.max_concurrent_sessions << "\n";
        std::cout << "  本期费用: $" << cost << "\n";
    }

private:
    UsageStats& get_or_create_stats_unlocked(const std::string& tenant_id) {
        auto it = stats_.find(tenant_id);
        if (it == stats_.end()) {
            stats_[tenant_id] = UsageStats{};
            stats_[tenant_id].tenant_id = tenant_id;
        }
        return stats_[tenant_id];
    }
    
    void check_quota_unlocked(const std::string& tenant_id) {
        // 简化：仅检查费用告警
        double cost = calculate_cost_unlocked(stats_[tenant_id]);
        if (cost >= config_.cost_alert_threshold) {
            create_alert_unlocked(tenant_id, "cost_threshold",
                "费用已超过 $" + std::to_string((int)config_.cost_alert_threshold));
        }
    }
    
    double calculate_cost_unlocked(const UsageStats& stats) {
        double cost = 0.0;
        cost += (stats.input_tokens / 1000.0) * config_.input_token_price;
        cost += (stats.output_tokens / 1000.0) * config_.output_token_price;
        cost += stats.ai_messages * config_.ai_message_price;
        cost += stats.human_messages * config_.human_message_price;
        cost += (stats.storage_mb / 1024.0) * config_.storage_price;
        return cost;
    }
    
    void create_alert_unlocked(const std::string& tenant_id,
                               const std::string& type,
                               const std::string& message) {
        BillingAlert alert;
        alert.alert_id = "alert_" + std::to_string(++alert_counter_);
        alert.tenant_id = tenant_id;
        alert.alert_type = type;
        alert.message = message;
        alert.created_at = std::chrono::system_clock::now();
        alert.is_resolved = false;
        
        alerts_.push_back(alert);
        
        std::cout << "[BillingService] 告警: " << type << " - " << message 
                  << " (租户: " << tenant_id << ")\n";
    }
    
    BillingConfig config_;
    std::map<std::string, UsageStats> stats_;
    std::vector<BillingAlert> alerts_;
    mutable std::mutex mutex_;
    int alert_counter_ = 0;
};

} // namespace nuclaw
