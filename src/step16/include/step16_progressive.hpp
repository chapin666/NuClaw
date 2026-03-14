// ============================================================================
// step16_progressive.hpp - Step 16: 渐进式演进汇总头文件
// 
// 本文件汇总 Step 16 的所有组件，方便渐进式引用
// 也可以单独引用各个组件：
//   #include "nuclaw/emotion.hpp"
//   #include "nuclaw/memory.hpp"
//   etc.
// ============================================================================

#pragma once

// Step 15 基线（引用已有代码）
#include "nuclaw/agent.hpp"

// Step 16 新增组件
#include "nuclaw/emotion.hpp"
#include "nuclaw/memory.hpp"
#include "nuclaw/relationship.hpp"
#include "nuclaw/agent_state.hpp"
#include "nuclaw/stateful_agent.hpp"

// 为了保持向后兼容，保留 step16 命名空间
namespace step16 {
    using namespace nuclaw;
}
