#!/bin/bash
# Step 1 测试脚本 - 测试并发性能

echo "=== NuClaw Step 1 Test ==="
echo ""

# 检查服务器是否运行
if ! curl -s http://localhost:8080 > /dev/null 2>&1; then
    echo "[!] Server not running"
    echo "    Please start: cd build && ./nuclaw_step01"
    exit 1
fi

echo "[+] Server is running"
echo ""

# 测试 1: 基本响应
echo "Test 1: Basic response"
RESPONSE=$(curl -s http://localhost:8080/)
if echo "$RESPONSE" | grep -q "step.*1"; then
    echo "    ✓ PASS"
else
    echo "    ✗ FAIL"
fi
echo ""

# 测试 2: 路由
echo "Test 2: Routing (/health)"
if curl -s http://localhost:8080/health | grep -q "threads"; then
    echo "    ✓ PASS"
else
    echo "    ✗ FAIL"
fi
echo ""

# 测试 3: 并发性能
echo "Test 3: Concurrent requests (10 parallel)"
START=$(date +%s%N)
for i in {1..10}; do
    curl -s http://localhost:8080/ > /dev/null &
done
wait
END=$(date +%s%N)
TIME=$(( (END - START) / 1000000 ))
echo "    ✓ Completed in ${TIME}ms"
echo ""

echo "=== Test Complete ==="
