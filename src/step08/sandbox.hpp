// ============================================================================
// sandbox.hpp - 安全沙箱
// ============================================================================

#pragma once

#include <filesystem>
#include <vector>
#include <string>

namespace fs = std::filesystem;

class Sandbox {
public:
    // 允许访问的路径白名单
    void allow_path(const fs::path& path) {
        fs::path p = path;
        if (fs::exists(p)) {
            allowed_paths_.push_back(fs::canonical(p));
        } else {
            // 如果路径不存在，使用绝对路径
            allowed_paths_.push_back(fs::absolute(p));
        }
    }
    
    // 检查路径是否在白名单内
    bool is_allowed(const fs::path& path) const {
        try {
            auto canon = fs::canonical(path);
            for (const auto& allowed : allowed_paths_) {
                auto [it, _] = std::mismatch(
                    allowed.begin(), allowed.end(),
                    canon.begin(), canon.end()
                );
                if (it == allowed.end()) return true;
            }
        } catch (...) {
            return false;
        }
        return false;
    }
    
    // 安全检查：防止路径遍历攻击
    static bool is_safe_path(const std::string& path) {
        if (path.find("..") != std::string::npos) return false;
        if (!path.empty() && path[0] == '/') return false;
        return true;
    }

private:
    std::vector<fs::path> allowed_paths_;
};
