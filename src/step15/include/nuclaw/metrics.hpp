// ============================================================================
// metrics.hpp - 指标监控（Step 13）
// ============================================================================

#pragma once

#include <string>
#include <map>
#include <math>
#include <mutex>
#include <sstream>
#include <atomic>

// Counter 计数器
class Counter {
public:
    void increment(double value = 1.0) {
        value_.fetch_add(value);
    }
    
    double get() const {
        return value_.load();
    }

private:
    std::atomic<double> value_{0};
};

// Gauge 仪表盘
class Gauge {
public:
    void set(double value) {
        value_.store(value);
    }
    
    void increment(double value = 1.0) {
        value_.fetch_add(value);
    }
    
    void decrement(double value = 1.0) {
        value_.fetch_sub(value);
    }
    
    double get() const {
        return value_.load();
    }

private:
    std::atomic<double> value_{0};
};

// Metrics 管理器
class Metrics {
public:
    static Metrics& instance() {
        static Metrics instance;
        return instance;
    }
    
    Counter* counter(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (counters_.find(name) == counters_.end()) {
            counters_[name] = std::make_unique<Counter>();
        }
        return counters_[name].get();
    }
    
    Gauge* gauge(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (gauges_.find(name) == gauges_.end()) {
            gauges_[name] = std::make_unique<Gauge>();
        }
        return gauges_[name].get();
    }
    
    std::string export_prometheus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::stringstream ss;
        
        for (const auto& [name, counter] : counters_) {
            ss << "# TYPE " << name << " counter\n";
            ss << name << " " << counter->get() << "\n";
        }
        
        for (const auto& [name, gauge] : gauges_) {
            ss << "# TYPE " << name << " gauge\n";
            ss << name << " " << gauge->get() << "\n";
        }
        
        return ss.str();
    }
    
    void print_all() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "=== Metrics ===\n";
        for (const auto& [name, counter] : counters_) {
            std::cout << name << ": " << counter->get() << "\n";
        }
        for (const auto& [name, gauge] : gauges_) {
            std::cout << name << ": " << gauge->get() << "\n";
        }
    }

private:
    mutable std::mutex mutex_;
    std::map<std::string, std::unique_ptr<Counter>> counters_;
    std::map<std::string, std::unique_ptr<Gauge>> gauges_;
};

#define METRICS_COUNTER(name) Metrics::instance().counter(name)
#define METRICS_GAUGE(name) Metrics::instance().gauge(name)
