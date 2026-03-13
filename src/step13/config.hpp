// ============================================================================
// config.hpp - 配置管理（Step 12）
// ============================================================================
// 支持：YAML/JSON 加载、环境变量、分层配置
// ============================================================================

#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <fstream>
#include <sstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/yaml_parser.hpp>

namespace pt = boost::property_tree;

class Config {
public:
    // 从 YAML 加载
    bool load_yaml(const std::string& path) {
        try {
            pt::ptree tree;
            pt::read_yaml(path, tree);
            merge_tree(tree);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "加载 YAML 失败: " << e.what() << std::endl;
            return false;
        }
    }
    
    // 从 JSON 加载
    bool load_json(const std::string& path) {
        try {
            pt::ptree tree;
            pt::read_json(path, tree);
            merge_tree(tree);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "加载 JSON 失败: " << e.what() << std::endl;
            return false;
        }
    }
    
    // 加载环境变量（NUCLAW_ 前缀）
    void load_env(const std::string& prefix = "NUCLAW_") {
        extern char** environ;
        for (char** env = environ; *env; ++env) {
            std::string env_str(*env);
            size_t pos = env_str.find('=');
            if (pos == std::string::npos) continue;
            
            std::string key = env_str.substr(0, pos);
            std::string value = env_str.substr(pos + 1);
            
            if (key.find(prefix) == 0) {
                std::string config_key = key.substr(prefix.length());
                std::transform(config_key.begin(), config_key.end(), 
                              config_key.begin(), ::tolower);
                std::replace(config_key.begin(), config_key.end(), '_', '.');
                set(config_key, value);
            }
        }
    }
    
    // 读取配置
    std::string get_string(const std::string& key, 
                           const std::string& default_val = "") const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = data_.find(key);
        return (it != data_.end()) ? it->second : default_val;
    }
    
    int get_int(const std::string& key, int default_val = 0) const {
        std::string val = get_string(key, "");
        if (val.empty()) return default_val;
        try {
            return std::stoi(val);
        } catch (...) {
            return default_val;
        }
    }
    
    double get_double(const std::string& key, double default_val = 0.0) const {
        std::string val = get_string(key, "");
        if (val.empty()) return default_val;
        try {
            return std::stod(val);
        } catch (...) {
            return default_val;
        }
    }
    
    bool get_bool(const std::string& key, bool default_val = false) const {
        std::string val = get_string(key, "");
        if (val == "true" || val == "1") return true;
        if (val == "false" || val == "0") return false;
        return default_val;
    }
    
    // 设置配置
    void set(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_[key] = value;
    }
    
    // 打印所有配置
    void print_all() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "=== 当前配置 ===\n";
        for (const auto& [k, v] : data_) {
            std::cout << k << " = " << v << "\n";
        }
    }

private:
    void merge_tree(const pt::ptree& tree, const std::string& prefix = "") {
        for (const auto& [key, value] : tree) {
            std::string full_key = prefix.empty() ? key : prefix + "." + key;
            
            if (value.empty()) {
                data_[full_key] = value.get_value<std::string>();
            } else {
                merge_tree(value, full_key);
            }
        }
    }
    
    mutable std::mutex mutex_;
    std::map<std::string, std::string> data_;
};
