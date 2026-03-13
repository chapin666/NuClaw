// ============================================================================
// logger.hpp - 日志系统（Step 13）
// ============================================================================

#pragma once

#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <chrono>

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

inline std::string level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

class Logger {
public:
    static Logger& instance() {
        static Logger instance;
        return instance;
    }
    
    void set_level(LogLevel level) {
        min_level_ = level;
    }
    
    void log(LogLevel level, const std::string& message,
             const std::string& file = "", int line = 0) {
        if (level < min_level_) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        
        std::cout << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
        std::cout << " [" << level_to_string(level) << "] ";
        std::cout << message;
        
        if (!file.empty()) {
            std::cout << " (" << file << ":" << line << ")";
        }
        
        std::cout << std::endl;
    }

private:
    Logger() = default;
    LogLevel min_level_ = LogLevel::INFO;
    std::mutex mutex_;
};

#define LOG_DEBUG(msg) Logger::instance().log(LogLevel::DEBUG, msg, __FILE__, __LINE__)
#define LOG_INFO(msg)  Logger::instance().log(LogLevel::INFO, msg, __FILE__, __LINE__)
#define LOG_WARN(msg)  Logger::instance().log(LogLevel::WARN, msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) Logger::instance().log(LogLevel::ERROR, msg, __FILE__, __LINE__)
