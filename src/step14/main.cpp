// ============================================================================
// main.cpp - Step 13: 监控可观测性演示
// ============================================================================

#include "metrics.hpp"
#include "logger.hpp"
#include <iostream>
#include <thread>
#include <chrono>

// 模拟处理请求
void handle_request(int id) {
    METRICS_GAUGE("active_requests")->increment();
    
    LOG_INFO("开始处理请求 #" + std::to_string(id));
    
    // 模拟处理时间
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    METRICS_COUNTER("requests_total")->increment();
    METRICS_GAUGE("active_requests")->decrement();
    
    LOG_INFO("完成处理请求 #" + std::to_string(id));
}

int main() {
    std::cout << "========================================\n";
    std::cout << "   NuClaw Step 13: 监控可观测性\n";
    std::cout << "========================================\n\n";
    
    // 设置日志级别
    Logger::instance().set_level(LogLevel::DEBUG);
    
    std::cout << "[1] 模拟处理请求...\n\n";
    
    // 模拟处理多个请求
    for (int i = 1; i <= 5; ++i) {
        handle_request(i);
    }
    
    std::cout << "\n[2] 指标数据:\n";
    Metrics::instance().print_all();
    
    std::cout << "\n[3] Prometheus 格式导出:\n";
    std::cout << Metrics::instance().export_prometheus() << std::endl;
    
    std::cout << "========================================\n";
    std::cout << "监控演示完成！\n";
    std::cout << "========================================\n";
    
    return 0;
}
