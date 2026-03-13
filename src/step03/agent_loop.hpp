// ============================================================================
// agent_loop.hpp - Agent 状态机核心
// ============================================================================
//
// 职责：
//   - 管理对话状态（IDLE, THINKING, TOOL_CALLING, RESPONDING）
//   - 维护对话历史
//   - 处理用户输入并生成响应
//
// 设计模式：状态机（State Machine）
// ============================================================================

#pragma once  // 防止头文件重复包含

#include <boost/json.hpp>
#include <map>
#include <vector>
#include <string>
#include <ctime>

namespace json = boost::json;

// ----------------------------------------------------------------------------
// Agent Loop 核心类
// ----------------------------------------------------------------------------
class AgentLoop {
public:
    // ---------------------------------------------------------------------
    // 状态定义
    // ---------------------------------------------------------------------
    // 为什么用 enum class？
    // - 类型安全：不能与其他整数混用
    // - 作用域：State::IDLE 而不是全局 IDLE
    // - 强类型：编译器会检查错误
    // ---------------------------------------------------------------------
    enum class State {
        IDLE,           // 空闲：等待用户输入
        THINKING,       // 思考中：处理输入、调用 LLM
        TOOL_CALLING,   // 调用工具：执行外部操作
        RESPONDING      // 响应中：生成回复
    };
    
    // ---------------------------------------------------------------------
    // 消息结构
    // ---------------------------------------------------------------------
    struct Message {
        std::string role;       // "user", "assistant", "system"
        std::string content;    // 消息内容
        std::string timestamp;  // 时间戳（用于调试和展示）
    };
    
    // ---------------------------------------------------------------------
    // 会话结构
    // ---------------------------------------------------------------------
    struct Session {
        std::string id;                     // 会话唯一标识
        State state = State::IDLE;          // 当前状态
        std::vector<Message> history;       // 对话历史
        size_t max_history = 100;           // 最大历史条数（防止内存无限增长）
        
        // 添加消息到历史
        void add_message(const std::string& role, const std::string& content);
        
    private:
        // 获取当前时间字符串
        std::string current_time() const;
    };
    
    // ---------------------------------------------------------------------
    // 核心接口
    // ---------------------------------------------------------------------
    
    // 处理用户输入（状态机入口）
    // @param user_input: 用户输入的文本
    // @param session_id: 会话 ID（用于区分不同用户）
    // @return: 响应文本
    std::string process(const std::string& user_input, 
                       const std::string& session_id);
    
    // 获取会话信息（用于调试）
    json::value get_session_info(const std::string& session_id) const;

private:
    // ---------------------------------------------------------------------
    // 内部实现
    // ---------------------------------------------------------------------
    
    // 生成响应（简化版：实际应该调用 LLM）
    std::string generate_response(const std::string& input, 
                                  const Session& session) const;
    
    // 状态转字符串（用于调试输出）
    static std::string state_to_string(State s);
    
    // 存储所有会话
    // key: session_id, value: Session
    std::map<std::string, Session> sessions_;
};
