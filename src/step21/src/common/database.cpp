#include "smartsupport/common/database.hpp"

namespace smartsupport {

Database::Database(const std::string& conn_str) : conn_str_(conn_str) {}

std::shared_ptr<pqxx::connection> Database::acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!pool_.empty()) {
        auto conn = pool_.back();
        pool_.pop_back();
        return conn;
    }
    
    return std::make_shared<pqxx::connection>(conn_str_);
}

void Database::release(std::shared_ptr<pqxx::connection> conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (conn && conn->is_open()) {
        pool_.push_back(conn);
    }
}

} // namespace smartsupport
