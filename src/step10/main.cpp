// ============================================================================
// Step 10: RAG 检索增强生成 - 基于 Step 8 演进
// ============================================================================
//
// 演进说明：
//   Step 8: 安全沙箱 + 数据库持久化
//   Step 10: + RAG 检索 + 向量数据库 + 增强生成
//
// 新增：
//   - vector_store.hpp:   向量存储与相似度检索
//   - embedding_client.hpp: 文本向量化（支持真实 Embedding API）
//   - document_processor.hpp: 文档分块与预处理
//   - 完整 RAG 流程: 检索相关文档 → 构建上下文 → LLM 生成回复
//
// 编译: mkdir build && cd build && cmake .. && make
// 运行: ./nuclaw_step10
// 测试: curl -X POST http://localhost:8080/rag -d '{"query":"北京景点"}'
// ============================================================================

#include "vector_store.hpp"
#include "embedding_client.hpp"
#include "document_processor.hpp"
#include "common/http_server.hpp"
#include "common/llm_http_client.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <cmath>

namespace json = boost::json;
using namespace nuclaw;

// RAG Agent 处理器
class RAGAgentHandler {
public:
    RAGAgentHandler() {
        // 初始化知识库
        init_knowledge_base();
    }
    
    // RAG 检索增强生成
    json::value handle_rag(const json::value& req) {
        try {
            if (!req.as_object().contains("query")) {
                return error_response("Missing 'query' field");
            }
            
            std::string query = std::string(req.at("query").as_string());
            int top_k = 3;
            if (req.as_object().contains("top_k")) {
                top_k = static_cast<int>(req.at("top_k").as_int64());
            }
            
            std::cout << "[🔍 RAG Query] " << query << std::endl;
            
            // 1. 向量化查询
            auto query_vec = embedding_.get_embedding(query);
            
            // 2. 检索相关文档
            auto start = std::chrono::steady_clock::now();
            auto results = store_.search(query_vec, top_k);
            auto retrieve_end = std::chrono::steady_clock::now();
            auto retrieve_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                retrieve_end - start).count();
            
            // 3. 构建上下文
            std::string context = build_context(results);
            
            // 4. 生成回复
            std::string reply = generate_with_context(query, context);
            auto generate_end = std::chrono::steady_clock::now();
            auto generate_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                generate_end - retrieve_end).count();
            
            // 构建响应
            json::object resp;
            resp["success"] = true;
            resp["query"] = query;
            resp["reply"] = reply;
            resp["context_used"] = !results.empty();
            resp["retrieve_ms"] = retrieve_ms;
            resp["generate_ms"] = generate_ms;
            resp["total_ms"] = retrieve_ms + generate_ms;
            
            // 检索详情
            json::array sources;
            for (const auto& r : results) {
                json::object src;
                src["doc_id"] = r.doc.id;
                src["text"] = r.doc.text.substr(0, 100) + (r.doc.text.length() > 100 ? "..." : "");
                src["similarity"] = std::round(r.score * 10000) / 10000.0;  // 4位小数
                sources.push_back(src);
            }
            resp["sources"] = sources;
            resp["source_count"] = sources.size();
            resp["timestamp"] = get_timestamp();
            
            return resp;
            
        } catch (const std::exception& e) {
            return error_response(e.what());
        }
    }
    
    // 仅检索（不生成）
    json::value handle_search(const json::value& req) {
        try {
            if (!req.as_object().contains("query")) {
                return error_response("Missing 'query' field");
            }
            
            std::string query = std::string(req.at("query").as_string());
            int top_k = 5;
            if (req.as_object().contains("top_k")) {
                top_k = static_cast<int>(req.at("top_k").as_int64());
            }
            
            std::cout << "[🔍 Search] " << query << std::endl;
            
            auto query_vec = embedding_.get_embedding(query);
            auto results = store_.search(query_vec, top_k);
            
            json::object resp;
            resp["success"] = true;
            resp["query"] = query;
            
            json::array docs;
            for (const auto& r : results) {
                json::object doc;
                doc["id"] = r.doc.id;
                doc["text"] = r.doc.text;
                doc["similarity"] = std::round(r.score * 10000) / 10000.0;
                docs.push_back(doc);
            }
            resp["results"] = docs;
            resp["total"] = docs.size();
            resp["timestamp"] = get_timestamp();
            
            return resp;
            
        } catch (const std::exception& e) {
            return error_response(e.what());
        }
    }
    
    // 添加文档到知识库
    json::value handle_add_document(const json::value& req) {
        try {
            if (!req.as_object().contains("id") || !req.as_object().contains("text")) {
                return error_response("Missing 'id' or 'text' field");
            }
            
            std::string id = std::string(req.at("id").as_string());
            std::string text = std::string(req.at("text").as_string());
            
            std::cout << "[📄 Add Doc] " << id << std::endl;
            
            // 文档分块
            auto chunks = processor_.split_text(text);
            int chunk_count = 0;
            
            for (size_t i = 0; i < chunks.size(); ++i) {
                auto vec = embedding_.get_embedding(chunks[i]);
                
                Document doc;
                doc.id = id + "_chunk" + std::to_string(i);
                doc.text = chunks[i];
                doc.embedding = vec;
                
                store_.add_document(doc);
                chunk_count++;
            }
            
            json::object resp;
            resp["success"] = true;
            resp["doc_id"] = id;
            resp["chunks_added"] = chunk_count;
            resp["total_docs"] = store_.size();
            resp["timestamp"] = get_timestamp();
            
            return resp;
            
        } catch (const std::exception& e) {
            return error_response(e.what());
        }
    }
    
    // 获取知识库状态
    json::value get_status() const {
        json::object resp;
        resp["success"] = true;
        resp["total_documents"] = store_.size();
        resp["embedding_provider"] = embedding_.is_configured() ? "api" : "mock";
        resp["endpoints"] = json::array({
            json::object({
                {"path", "/rag"},
                {"method", "POST"},
                {"description", "RAG 检索增强生成"},
                {"params", "query, top_k(optional)"}
            }),
            json::object({
                {"path", "/search"},
                {"method", "POST"},
                {"description", "向量检索（仅返回相关文档）"},
                {"params", "query, top_k(optional)"}
            }),
            json::object({
                {"path", "/add"},
                {"method", "POST"},
                {"description", "添加文档到知识库"},
                {"params", "id, text"}
            })
        });
        return resp;
    }

private:
    VectorStore store_;
    EmbeddingClient embedding_;
    DocumentProcessor processor_;
    
    void init_knowledge_base() {
        std::cout << "[📚 Initializing Knowledge Base...]" << std::endl;
        
        // 预设知识库
        std::vector<std::pair<std::string, std::string>> knowledge = {
            {"beijing", "北京是中国的首都，人口约 2100 万。著名景点有故宫、长城、天安门、颐和园、天坛等。美食有北京烤鸭、炸酱面、豆汁等。"},
            {"shanghai", "上海是中国最大的城市，人口约 2400 万。著名景点有外滩、东方明珠、豫园、南京路等。美食有小笼包、生煎包、本帮菜等。"},
            {"python", "Python 是一种流行的编程语言，由 Guido van Rossum 于 1991 年创建。语法简洁，广泛应用于数据科学、AI、Web 开发等领域。"},
            {"cpp", "C++ 是一种高性能编程语言，支持面向对象编程和底层内存操作。常用于系统编程、游戏开发、嵌入式系统等。"},
            {"ml", "机器学习是人工智能的一个分支，通过数据训练模型来完成任务。常见算法包括神经网络、决策树、支持向量机等。"},
            {"nuclaw", "NuClaw 是一个 C++ AI Agent 教程项目，从零开始构建智能客服系统。涵盖 WebSocket、LLM、RAG、工具调用、多租户等技术。"}
        };
        
        for (const auto& [id, text] : knowledge) {
            auto chunks = processor_.split_text(text);
            for (size_t i = 0; i < chunks.size(); ++i) {
                auto vec = embedding_.get_embedding(chunks[i]);
                Document doc;
                doc.id = id + "_chunk" + std::to_string(i);
                doc.text = chunks[i];
                doc.embedding = vec;
                store_.add_document(doc);
            }
        }
        
        std::cout << "[✓] Knowledge base ready: " << store_.size() << " chunks" << std::endl << std::endl;
    }
    
    std::string build_context(const std::vector<SearchResult>& results) {
        if (results.empty()) return "";
        
        std::string context = "相关文档：\n";
        for (size_t i = 0; i < results.size(); ++i) {
            context += "[" + std::to_string(i + 1) + "] " + results[i].doc.text + "\n";
        }
        return context;
    }
    
    std::string generate_with_context(const std::string& query, const std::string& context) {
        // 尝试使用真实 LLM
        LLMHttpClient llm;
        if (llm.is_configured()) {
            std::string prompt = context.empty() 
                ? "问题：" + query + "\n请回答。"
                : "根据以下信息回答问题：\n" + context + "\n问题：" + query + "\n请基于上述信息回答。";
            
            auto response = llm.chat(prompt);
            if (response.success) {
                return response.content;
            }
        }
        
        // 回退到 Mock 生成
        if (context.empty()) {
            return "我没有找到相关信息。知识库包含：北京、上海、Python、C++、机器学习、NuClaw 等内容。";
        }
        
        // 简单的模板回复
        return "根据检索结果：" + context.substr(0, context.find('\n', context.find('\n') + 1)) + "...";
    }
    
    json::value error_response(const std::string& msg) {
        json::object resp;
        resp["error"] = msg;
        resp["success"] = false;
        return resp;
    }
    
    std::string get_timestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
};

int main() {
    try {
        boost::asio::io_context io;
        
        std::cout << "========================================" << std::endl;
        std::cout << "  NuClaw Step 10 - RAG Retrieval" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << std::endl;
        
        nuclaw::http::Server server(io, 8080);
        RAGAgentHandler handler;
        
        // 注册路由
        server.post("/rag", [&handler](const json::value& req) {
            return handler.handle_rag(req);
        });
        
        server.post("/search", [&handler](const json::value& req) {
            return handler.handle_search(req);
        });
        
        server.post("/add", [&handler](const json::value& req) {
            return handler.handle_add_document(req);
        });
        
        server.get("/status", [&handler](const json::value&) {
            return handler.get_status();
        });
        
        server.get("/health", [](const json::value&) {
            json::object resp;
            resp["status"] = "ok";
            resp["step"] = 10;
            resp["features"] = json::array({"rag", "vector_search", "document_add"});
            return json::value(resp);
        });
        
        server.start();
        
        std::cout << "✅ REST API Server started on port 8080!" << std::endl;
        std::cout << std::endl;
        
        std::cout << "📊 演进对比：" << std::endl;
        std::cout << "  Step 8: 安全沙箱 + 数据库持久化" << std::endl;
        std::cout << "  Step 10: + RAG 检索 + 向量数据库" << std::endl;
        std::cout << std::endl;
        
        std::cout << "📡 Endpoints:" << std::endl;
        std::cout << "  POST /rag       - RAG 检索增强生成" << std::endl;
        std::cout << "  POST /search    - 向量检索" << std::endl;
        std::cout << "  POST /add       - 添加文档" << std::endl;
        std::cout << "  GET  /status    - 知识库状态" << std::endl;
        std::cout << "  GET  /health    - 健康检查" << std::endl;
        std::cout << std::endl;
        
        std::cout << "🧪 Test:" << std::endl;
        std::cout << "  curl -X POST http://localhost:8080/rag \\" << std::endl;
        std::cout << "    -H 'Content-Type: application/json' \\" << std::endl;
        std::cout << "    -d '{\"query\":\"北京有什么景点\"}'" << std::endl;
        std::cout << std::endl;
        
        std::cout << "  curl -X POST http://localhost:8080/search \\" << std::endl;
        std::cout << "    -d '{\"query\":\"Python\",\"top_k\":3}'" << std::endl;
        std::cout << std::endl;
        
        std::cout << "📚 演进：" << std::endl;
        std::cout << "  Step 8 → 10: + 向量检索 + 知识增强" << std::endl;
        std::cout << "  下一章：配置管理" << std::endl;
        std::cout << std::endl;
        
        io.run();
        
    } catch (const std::exception& e) {
        std::cerr << "[!] Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
