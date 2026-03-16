#pragma once

#include <boost/asio.hpp>
#include <string>
#include <queue>
#include <map>
#include <mutex>
#include <optional>

namespace smartsupport::services::human {

namespace asio = boost::asio;

enum class AgentStatus {
    OFFLINE,
    ONLINE,
    BUSY,
    AWAY
};

struct HumanAgent {
    std::string id;
    std::string name;
    std::string tenant_id;
    AgentStatus status = AgentStatus::OFFLINE;
    int max_sessions = 5;
    int current_sessions = 0;
    std::vector<std::string> skills;
};

struct QueuedSession {
    std::string session_id;
    std::string tenant_id;
    std::string user_id;
    std::chrono::steady_clock::time_point queued_at;
    int priority = 0;
    std::vector<std::string> required_skills;
};

class HumanService {
public:
    HumanService(asio::io_context& io);
    
    void agent_login(const HumanAgent& agent);
    void agent_logout(const std::string& agent_id);
    void update_status(const std::string& agent_id, AgentStatus status);
    
    void request_human(const QueuedSession& session);
    void agent_take_session(const std::string& agent_id, const std::string& session_id);
    void agent_send_message(const std::string& agent_id, const std::string& session_id, 
                            const std::string& content);
    void agent_end_session(const std::string& agent_id, const std::string& session_id);
    
    struct Dashboard {
        std::vector<std::string> active_sessions;
        std::vector<QueuedSession> waiting_sessions;
    };
    Dashboard get_agent_dashboard(const std::string& agent_id);

private:
    std::optional<std::string> find_best_agent(const QueuedSession& session);
    void assign_session(const std::string& agent_id, const std::string& session_id);
    void notify_user_assigned(const std::string& session_id, const std::string& agent_name);
    void notify_user_queued(const std::string& session_id, int position);
    void notify_user_timeout(const std::string& session_id);
    void process_queue();
    
    std::map<std::string, HumanAgent> agents_;
    std::map<std::string, std::vector<std::string>> agent_sessions_;
    std::queue<QueuedSession> waiting_queue_;
    std::map<std::string, std::string> session_agent_;
    asio::steady_timer queue_timer_;
    std::mutex mutex_;
};

} // namespace smartsupport::services::human
