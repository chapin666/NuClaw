// ============================================================================
// tool.hpp - Tool 基类
// ============================================================================

#pragma once

#include <boost/json.hpp>
#include <string>

namespace json = boost::json;

class Tool {
public:
    virtual ~Tool() = default;
    virtual std::string get_name() const = 0;
    virtual std::string get_description() const = 0;
    virtual json::value execute(const json::object& args) = 0;
    
    json::value execute_safe(const json::object& args) {
        try {
            return execute(args);
        } catch (const std::exception& e) {
            json::object error;
            error["success"] = false;
            error["error"] = e.what();
            return error;
        }
    }
};
