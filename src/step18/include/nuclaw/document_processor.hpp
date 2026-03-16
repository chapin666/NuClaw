// ============================================================================
// document_processor.hpp - 文档处理（文本分块）
// ============================================================================

#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

class DocumentProcessor {
public:
    struct ChunkConfig {
        size_t chunk_size = 100;        // 每块字符数
        size_t chunk_overlap = 20;      // 重叠字符数
        std::string separator = "\n\n"; // 优先分隔符
    };
    
    // 将长文本分块
    std::vector<std::string> split_text(const std::string& text,
                                          const ChunkConfig& config = {}) {
        std::vector<std::string> chunks;
        
        // 优先按段落分割
        std::vector<std::string> paragraphs = split_by_separator(text, config.separator);
        
        std::string current_chunk;
        for (const auto& para : paragraphs) {
            if (current_chunk.length() + para.length() > config.chunk_size) {
                if (!current_chunk.empty()) {
                    chunks.push_back(current_chunk);
                }
                
                // 保留重叠部分
                if (current_chunk.length() > config.chunk_overlap) {
                    current_chunk = current_chunk.substr(
                        current_chunk.length() - config.chunk_overlap);
                } else {
                    current_chunk.clear();
                }
            }
            
            if (!current_chunk.empty()) {
                current_chunk += config.separator;
            }
            current_chunk += para;
        }
        
        if (!current_chunk.empty()) {
            chunks.push_back(current_chunk);
        }
        
        return chunks;
    }

private:
    std::vector<std::string> split_by_separator(const std::string& text,
                                                  const std::string& sep) {
        std::vector<std::string> parts;
        size_t start = 0;
        size_t end = text.find(sep);
        
        while (end != std::string::npos) {
            parts.push_back(text.substr(start, end - start));
            start = end + sep.length();
            end = text.find(sep, start);
        }
        
        parts.push_back(text.substr(start));
        return parts;
    }
};
