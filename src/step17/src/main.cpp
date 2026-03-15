#include <boost/asio.hpp>
#include <iostream>
#include "smartsupport/services/chat/service.hpp"
#include "smartsupport/services/ai/service.hpp"
#include "smartsupport/services/knowledge/service.hpp"

namespace smartsupport {

namespace asio = boost::asio;

class Application {
public:
    Application() : io_() {}
    
    void initialize() {
        // 初始化知识服务
        knowledge::KnowledgeService::Config kb_config{
            .vector_db_url = "http://localhost:8000",
            .embedding_model = "text-embedding-ada-002"
        };
        knowledge_service_ = std::make_shared<knowledge::KnowledgeService>(kb_config);
        
        // 初始化 AI 服务
        services::ai::AIService::Config ai_config{
            .llm_provider = "openai",
            .api_key = std::getenv("OPENAI_API_KEY") ?: "",
            .model = "gpt-4"
        };
        ai_service_ = std::make_shared<services::ai::AIService>(
            io_, ai_config, knowledge_service_);
        
        // 初始化 Chat 服务
        chat_service_ = std::make_shared<chat::ChatService>(
            io_, ai_service_, knowledge_service_);
        
        std::cout << "SmartSupport Step 17 initialized!\n";
    }
    
    void run() {
        std::cout << "Architecture design phase - no server running yet.\n";
        std::cout << "Step 17 focuses on architecture design and project structure.\n";
        std::cout << "Run Step 18+ for functional implementation.\n";
    }

private:
    asio::io_context io_;
    std::shared_ptr<knowledge::KnowledgeService> knowledge_service_;
    std::shared_ptr<services::ai::AIService> ai_service_;
    std::shared_ptr<chat::ChatService> chat_service_;
};

} // namespace smartsupport

int main(int argc, char* argv[]) {
    try {
        smartsupport::Application app;
        app.initialize();
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
