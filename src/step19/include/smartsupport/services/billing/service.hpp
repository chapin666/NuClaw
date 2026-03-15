#pragma once

#include <string>
#include <map>
#include <chrono>
#include <memory>

namespace smartsupport::services::billing {

enum class BillingItem {
    AI_CALL,
    MESSAGE,
    STORAGE,
    HUMAN_AGENT
};

struct UsageRecord {
    std::string tenant_id;
    BillingItem item;
    int amount;
    std::chrono::system_clock::time_point timestamp;
    std::string session_id;
};

struct UsageSummary {
    std::map<BillingItem, int> used;
    std::map<BillingItem, int> quota;
    std::map<BillingItem, double> overage_cost;
    double total_cost;
};

struct Invoice {
    std::string id;
    std::string tenant_id;
    std::string period;
    double amount;
    std::map<BillingItem, int> breakdown;
    bool paid;
};

class Database;

class BillingService {
public:
    BillingService(std::shared_ptr<Database> db, asio::io_context& io);
    
    void record_usage(const std::string& tenant_id, BillingItem item,
                      int amount, const std::string& session_id = "");
    UsageSummary get_current_usage(const std::string& tenant_id);
    Invoice generate_invoice(const std::string& tenant_id, const std::string& period);
    bool is_payment_overdue(const std::string& tenant_id);

private:
    void flush_usage_buffer();
    
    std::shared_ptr<Database> db_;
    std::vector<UsageRecord> usage_buffer_;
    std::mutex buffer_mutex_;
    asio::steady_timer flush_timer_;
};

} // namespace smartsupport::services::billing
