# Step 12: 配置管理 - YAML/JSON 配置与热加载

> 目标：实现外部化配置，支持热加载，无需重启修改参数
> 
> 难度：⭐⭐⭐⭐ (较难)
> 
> 代码量：约 900 行
> 
> 预计学习时间：4-5 小时

---

## 📚 前置知识

### 为什么需要配置管理？

**硬编码配置的问题：**

```cpp
// ❌ 不好的做法 - 硬编码
class LLMClient {
    std::string api_key_ = "sk-abc123";  // 密钥泄露在代码中！
    std::string model_ = "gpt-4";        // 改模型要重新编译
    int timeout_ = 30;                   // 无法动态调整
};
```

**实际痛点：**

| 场景 | 问题 | 影响 |
|:---|:---|:---|
| **密钥泄露** | API key 在代码中 | 安全风险 |
| **环境切换** | 开发/生产配置不同 | 频繁改代码 |
| **参数调优** | 调整超时时间 | 需重启服务 |
| **团队协作** | 个人配置冲突 | 代码冲突 |

### 配置管理的目标

**1. 配置与代码分离**
```
配置 → 配置文件/环境变量
代码 → 读取配置

好处：改配置不碰代码
```

**2. 多环境支持**
```
开发环境：config.dev.yaml
测试环境：config.test.yaml
生产环境：config.prod.yaml
```

**3. 动态调整**
```
运行中修改配置 → 自动生效 → 无需重启
```

### 配置优先级

**优先级从高到低：**

```
1. 命令行参数（最优先）
   --port=8080

2. 环境变量
   export NUCLAW_PORT=8080

3. 配置文件
   port: 8080

4. 代码默认值（最低）
   int port = 8080;
```

**为什么这样设计？**
- 命令行：临时覆盖，一次性
- 环境变量：容器化部署常用
- 配置文件：主要配置方式
- 代码默认值：保底，确保能运行

### 配置文件格式选择

| 格式 | 优点 | 缺点 | 推荐场景 |
|:---|:---|:---|:---|
| **YAML** | 可读性好，支持注释 | 缩进敏感 | 复杂配置 |
| **JSON** | 通用，解析快 | 不支持注释 | 机器交互 |
| **TOML** | 简洁，明确 | 较新 | Rust 项目 |
| **INI** | 简单 | 不支持嵌套 | 简单配置 |

**推荐：** YAML 为主，JSON 为辅

---

## 第一步：配置分层设计

### 配置结构

```yaml
# config.yaml - 完整配置示例

# 服务器配置
server:
  host: "0.0.0.0"
  port: 8080
  workers: 4

# LLM 配置
llm:
  provider: "openai"      # openai, anthropic, azure
  api_key: "${OPENAI_API_KEY}"  # 从环境变量读取
  model: "gpt-4"
  timeout: 30
  max_tokens: 2000
  temperature: 0.7

# 工具配置
tools:
  enabled:
    - weather
    - time
    - calculator
  
  weather:
    api_key: "${WEATHER_API_KEY}"
    default_city: "北京"

# RAG 配置
rag:
  enabled: true
  embedding_model: "text-embedding-3-small"
  top_k: 3

# 日志配置
logging:
  level: "info"           # debug, info, warn, error
  format: "json"          # text, json
  output: "stdout"        # stdout, file
```

### 配置读取方式

```cpp
// 读取嵌套配置
std::string api_key = config.get_string("llm.api_key");
int timeout = config.get_int("llm.timeout", 30);  // 带默认值
double temp = config.get_double("llm.temperature", 0.7);
bool rag_enabled = config.get_bool("rag.enabled", true);

// 读取数组
std::vector<std::string> tools = config.get_array("tools.enabled");
```

---

## 第二步：配置加载策略

### 配置加载流程

```
1. 加载基础配置（config.yaml）
         ↓
2. 加载环境特定配置（config.prod.yaml）覆盖
         ↓
3. 加载环境变量覆盖
         ↓
4. 加载命令行参数覆盖
         ↓
5. 配置生效
```

### 环境变量映射

```bash
# 环境变量命名规范
NUCLAW_LLM_API_KEY      → llm.api_key
NUCLAW_SERVER_PORT      → server.port
NUCLAW_RAG_ENABLED      → rag.enabled

# 转换规则：
# 1. 去掉前缀 NUCLAW_
# 2. 转小写
# 3. 下划线变点
```

### 敏感信息处理

**问题：** API key 不应该提交到 Git

**解决方案：**
```yaml
# config.yaml（提交到 Git）
llm:
  api_key: "${OPENAI_API_KEY}"  # 占位符

# .env 文件（不提交到 Git）
OPENAI_API_KEY=sk-abc123

# 程序启动时加载 .env
```

---

## 第三步：热加载机制

### 什么是热加载？

**传统方式：**
```
修改配置 → 重启服务 → 配置生效
              ↓
         服务中断，用户受影响
```

**热加载方式：**
```
修改配置 → 自动检测 → 配置生效
              ↓
         无中断，用户无感知
```

### 热加载实现原理

**文件监听：**
```
Linux: inotify
macOS: FSEvents
Windows: ReadDirectoryChangesW

通用方案：轮询（每秒检查修改时间）
```

**加载流程：**
```
1. 检测到文件修改
2. 读取新配置
3. 验证配置合法性
4. 通知相关组件更新
5. 记录配置变更日志
```

### 热加载注意事项

**不能热加载的配置：**
```
- 端口号（需要重新绑定）
- 数据库连接（需要重建连接池）
- 线程池大小（需要重新创建）
```

**可以热加载的配置：**
```
- 超时时间
- 日志级别
- 功能开关
- 阈值参数
```

---

## 第四步：配置验证

### 为什么需要验证？

**无效配置的后果：**
```
- port = abc     → 程序崩溃
- timeout = -1   → 无限等待
- api_key = ""   → 认证失败
```

### 验证策略

**Schema 验证：**
```yaml
# 定义配置 schema
server.port:
  type: integer
  min: 1
  max: 65535
  required: true

llm.temperature:
  type: float
  min: 0.0
  max: 2.0
  default: 0.7
```

**运行时验证：**
```cpp
void validate_config(const Config& cfg) {
    // 检查必需项
    if (cfg.get_string("llm.api_key").empty()) {
        throw ConfigError("llm.api_key 不能为空");
    }
    
    // 检查范围
    int port = cfg.get_int("server.port");
    if (port < 1 || port > 65535) {
        throw ConfigError("server.port 范围 1-65535");
    }
}
```

---

## 第五节：最佳实践

### 1. 配置即文档

```yaml
# ❌ 不好的配置
retry: 3
timeout: 30

# ✅ 好的配置
# 请求失败时的重试次数
retry_count: 3

# HTTP 请求超时时间（秒）
http_timeout_seconds: 30
```

### 2. 合理的默认值

```cpp
// 确保没有配置也能运行
int port = config.get_int("server.port", 8080);
std::string level = config.get_string("log.level", "info");
```

### 3. 配置变更审计

```
记录内容：
- 谁修改了配置
- 修改了什么
- 修改时间
- 旧值 → 新值
```

### 4. 配置加密

```yaml
# 敏感配置加密存储
llm:
  api_key: "ENC(encrypted_base64_string)"

# 程序解密后使用
```

---

## 本节总结

### 核心概念

1. **配置分层**：配置文件 → 环境变量 → 命令行参数
2. **热加载**：运行中更新配置，无需重启
3. **配置验证**：确保配置合法有效
4. **敏感信息**：环境变量或加密存储

### 配置管理流程

```
开发阶段：编写 config.yaml 模板
部署阶段：填充环境特定值
运行阶段：监听变更，热加载
```

### 工具推荐

| 用途 | 工具 |
|:---|:---|
| 配置中心 | Consul, Etcd, Apollo |
| 密钥管理 | Vault, AWS Secrets Manager |
| 配置验证 | JSON Schema, Cue |

---

## 📝 课后练习

### 练习 1：配置模板
实现配置模板渲染，支持变量替换。

### 练习 2：配置对比
实现配置变更 diff 功能，显示修改内容。

### 练习 3：配置回滚
保存配置历史，支持回滚到上一版本。

### 思考题
1. 热加载时如何处理正在进行的请求？
2. 分布式系统的配置一致性如何保证？
3. 配置变更如何通知所有相关节点？

---

**恭喜！** 你的 Agent 现在支持配置管理和热加载了。下一章我们将添加监控和告警功能。
