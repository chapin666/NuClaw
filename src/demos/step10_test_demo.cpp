// ============================================================================
// test_step10.cpp - Step 10: 测试策略
// ============================================================================
// 演示内容：
// 1. Google Test 基础
// 2. 单元测试（组件测试）
// 3. 集成测试（完整流程）
// 4. Mock 对象（模拟外部依赖）
// ============================================================================

#include <gtest/gtest.h>
#include "nuclaw/common/http_client.hpp"
#include "nuclaw/common/llm_http_client.hpp"
#include <boost/json.hpp>
#include <chrono>
#include <thread>

namespace json = boost::json;
using namespace nuclaw;

// ============================================================================
// 第一部分：单元测试 - 基础组件
// ============================================================================

// HTTP 响应测试
TEST(HttpResponseTest, DefaultValues) {
    http::Response resp;
    EXPECT_EQ(resp.status_code, 0);
    EXPECT_FALSE(resp.success);
    EXPECT_TRUE(resp.error.empty());
    EXPECT_TRUE(resp.body.empty());
}

TEST(HttpResponseTest, OkStatus) {
    http::Response resp;
    resp.status_code = 200;
    EXPECT_TRUE(resp.ok());
    
    resp.status_code = 201;
    EXPECT_TRUE(resp.ok());
    
    resp.status_code = 404;
    EXPECT_FALSE(resp.ok());
    
    resp.status_code = 500;
    EXPECT_FALSE(resp.ok());
}

// LLM 配置测试
TEST(LLMConfigTest, FromEnv_DefaultValues) {
    // 清除环境变量
    unsetenv("OPENAI_API_KEY");
    unsetenv("LLM_PROVIDER");
    
    LLMHttpClient::Config config;
    config.provider = "openai";
    config.model = "gpt-3.5-turbo";
    
    EXPECT_EQ(config.provider, "openai");
    EXPECT_EQ(config.model, "gpt-3.5-turbo");
    EXPECT_TRUE(config.api_key.empty());
}

TEST(LLMConfigTest, Response_DefaultValues) {
    LLMResponse resp;
    EXPECT_FALSE(resp.success);
    EXPECT_EQ(resp.status_code, 0);
    EXPECT_EQ(resp.tokens_used, 0);
    EXPECT_DOUBLE_EQ(resp.latency_ms, 0.0);
}

// ============================================================================
// 第二部分：Mock 对象 - 模拟外部依赖
// ============================================================================

// Mock LLM 客户端（用于测试）
class MockLLMClient {
public:
    struct Response {
        std::string content;
        int tokens_used;
        bool success;
    };
    
    std::vector<std::pair<std::string, std::string>> history_;
    
    Response chat(const std::string& message) {
        history_.push_back({"user", message});
        
        // 模拟回复逻辑
        Response resp;
        resp.success = true;
        resp.tokens_used = static_cast<int>(message.length() + 20);
        
        if (message.find("天气") != std::string::npos) {
            resp.content = "今天天气晴朗，气温25°C。";
        } else if (message.find("时间") != std::string::npos) {
            resp.content = "当前时间是 2024年1月1日 12:00";
        } else {
            resp.content = "收到你的消息: " + message;
        }
        
        history_.push_back({"assistant", resp.content});
        return resp;
    }
};

// Mock 数据库（用于测试）
class MockDatabase {
public:
    struct Message {
        std::string session_id;
        std::string role;
        std::string content;
    };
    
    std::vector<Message> messages_;
    int query_count_ = 0;
    
    void save_message(const std::string& session_id,
                      const std::string& role,
                      const std::string& content) {
        messages_.push_back({session_id, role, content});
    }
    
    std::vector<Message> get_history(const std::string& session_id) {
        query_count_++;
        std::vector<Message> result;
        for (const auto& m : messages_) {
            if (m.session_id == session_id) {
                result.push_back(m);
            }
        }
        return result;
    }
    
    size_t count() const { return messages_.size(); }
    void clear() { messages_.clear(); query_count_ = 0; }
};

// ============================================================================
// 第三部分：集成测试 - 完整业务逻辑
// ============================================================================

// 对话服务（用于测试完整流程）
class ChatService {
public:
    ChatService(MockLLMClient& llm, MockDatabase& db) 
        : llm_(llm), db_(db) {}
    
    struct ChatResult {
        std::string reply;
        std::string session_id;
        bool success;
    };
    
    ChatResult chat(const std::string& session_id, 
                    const std::string& user_id,
                    const std::string& message) {
        ChatResult result;
        result.session_id = session_id;
        
        // 保存用户消息
        db_.save_message(session_id, "user", message);
        
        // 调用 LLM
        auto llm_resp = llm_.chat(message);
        
        if (llm_resp.success) {
            result.reply = llm_resp.content;
            result.success = true;
            
            // 保存助手回复
            db_.save_message(session_id, "assistant", llm_resp.content);
        } else {
            result.reply = "Error";
            result.success = false;
        }
        
        return result;
    }
    
    size_t get_message_count(const std::string& session_id) {
        return db_.get_history(session_id).size();
    }

private:
    MockLLMClient& llm_;
    MockDatabase& db_;
};

// 集成测试
class ChatServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        llm_ = std::make_unique<MockLLMClient>();
        db_ = std::make_unique<MockDatabase>();
        service_ = std::make_unique<ChatService>(*llm_, *db_);
    }
    
    void TearDown() override {
        db_->clear();
    }
    
    std::unique_ptr<MockLLMClient> llm_;
    std::unique_ptr<MockDatabase> db_;
    std::unique_ptr<ChatService> service_;
};

TEST_F(ChatServiceTest, BasicChat) {
    auto result = service_->chat("sess_001", "user_001", "你好");
    
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.reply.empty());
    EXPECT_EQ(result.session_id, "sess_001");
}

TEST_F(ChatServiceTest, ChatSavesToDatabase) {
    service_->chat("sess_001", "user_001", "今天天气如何?");
    
    // 验证保存了用户消息和助手回复
    EXPECT_EQ(db_->count(), 2);
    
    // 验证可以获取历史
    auto history = db_->get_history("sess_001");
    EXPECT_EQ(history.size(), 2);
    EXPECT_EQ(history[0].role, "user");
    EXPECT_EQ(history[1].role, "assistant");
}

TEST_F(ChatServiceTest, MultipleSessions) {
    service_->chat("sess_001", "user_001", "消息A");
    service_->chat("sess_002", "user_002", "消息B");
    service_->chat("sess_001", "user_001", "消息C");
    
    EXPECT_EQ(db_->count(), 6);  // 3个对话 x 2条消息
    
    auto history1 = db_->get_history("sess_001");
    auto history2 = db_->get_history("sess_002");
    
    EXPECT_EQ(history1.size(), 4);  // sess_001 有4条消息
    EXPECT_EQ(history2.size(), 2);  // sess_002 有2条消息
}

TEST_F(ChatServiceTest, WeatherQuery) {
    auto result = service_->chat("sess_001", "user_001", "北京天气如何?");
    
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.reply.find("天气"), std::string::npos);
    EXPECT_NE(result.reply.find("°C"), std::string::npos);
}

TEST_F(ChatServiceTest, ContextPreserved) {
    // 连续对话，LLM 应该能访问历史
    service_->chat("sess_001", "user_001", "问题1");
    service_->chat("sess_001", "user_001", "问题2");
    
    // 验证 LLM 历史中有之前的对话
    EXPECT_EQ(llm_->history_.size(), 4);  // 2轮 x 2条消息
}

// ============================================================================
// 第四部分：性能测试
// ============================================================================

TEST(PerformanceTest, ChatLatency) {
    MockLLMClient llm;
    MockDatabase db;
    ChatService service(llm, db);
    
    auto start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < 100; ++i) {
        service.chat("sess_" + std::to_string(i), "user_001", "测试消息");
    }
    
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // 100次调用应该在100ms内完成（Mock非常快）
    EXPECT_LT(elapsed, 100);
    std::cout << "\n[Performance] 100 chats in " << elapsed << " ms\n";
}

// ============================================================================
// 主函数
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "========================================\n";
    std::cout << "  NuClaw Step 10 - Testing Strategy\n";
    std::cout << "========================================\n\n";
    
    std::cout << "测试类别:\n";
    std::cout << "  1. 单元测试: HTTP客户端, LLM配置\n";
    std::cout << "  2. Mock测试: MockLLM, MockDatabase\n";
    std::cout << "  3. 集成测试: ChatService 完整流程\n";
    std::cout << "  4. 性能测试: 延迟基准\n\n";
    
    return RUN_ALL_TESTS();
}
