#!/bin/bash
# examples/test_step00.sh
# Step 0 自动化测试脚本

set -e  # 遇到错误立即退出

echo "========================================"
echo "  NuClaw Step 0 - Test Script"
echo "========================================"
echo ""

# 检查服务器是否已运行
if pgrep -x "nuclaw_step00" > /dev/null; then
    echo "[!] Server already running, stopping it first"
    pkill -x "nuclaw_step00" || true
    sleep 1
fi

# 编译
echo "[1/4] Building..."
cd build || (mkdir build && cd build)
cmake .. > /dev/null 2>&1
make nuclaw_step00 -j$(nproc) > /dev/null 2>&1
echo "      ✓ Build successful"

# 启动服务器
echo "[2/4] Starting server..."
./nuclaw_step00 &
SERVER_PID=$!
sleep 1
echo "      ✓ Server started (PID: $SERVER_PID)"

# 测试连接
echo "[3/4] Testing..."
RESPONSE=$(curl -s http://localhost:8080 || echo "FAILED")

if echo "$RESPONSE" | grep -q "Hello from NuClaw"; then
    echo "      ✓ Test passed"
    echo ""
    echo "Response:"
    echo "$RESPONSE" | python3 -m json.tool 2>/dev/null || echo "$RESPONSE"
else
    echo "      ✗ Test failed"
    echo "Response: $RESPONSE"
    kill $SERVER_PID 2>/dev/null || true
    exit 1
fi

# 停止服务器
echo ""
echo "[4/4] Stopping server..."
kill $SERVER_PID 2>/dev/null || true
echo "      ✓ Server stopped"

echo ""
echo "========================================"
echo "  All tests passed!"
echo "========================================"
