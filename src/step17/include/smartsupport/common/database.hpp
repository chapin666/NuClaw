#pragma once

#include <string>
#include <pqxx/pqxx>
#include <memory>

namespace smartsupport {

// 数据库连接池
class Database {
public:
    Database(const std::string& conn_str);
    
    std::shared_ptr<pqxx::connection> acquire();
    void release(std::shared_ptr<pqxx::connection> conn);

private:
    std::string conn_str_;
    std::vector<std::shared_ptr<pqxx::connection>> pool_;
    std::mutex mutex_;
};

} // namespace smartsupport
