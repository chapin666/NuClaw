#!/bin/bash
# Step 0 测试脚本

echo "=== NuClaw Step 0 Test ==="
echo ""

# 检查服务器是否运行
if ! curl -s http://localhost:8080 > /dev/null; then
    echo "[!] Server not running"
    echo "    Please start: cd src/step00 && ./server"
    exit 1
fi

echo "[+] Server is running"
echo ""

# 测试 1: 基本响应
echo "Test 1: Basic response"
RESPONSE=$(curl -s http://localhost:8080)
if echo "$RESPONSE" | grep -q "NuClaw"; then
    echo "    ✓ PASS"
else
    echo "    ✗ FAIL"
fi
echo ""

# 测试 2: JSON 格式
echo "Test 2: JSON format"
if echo "$RESPONSE" | grep -q '"status": "ok"'; then
    echo "    ✓ PASS"
else
    echo "    ✗ FAIL"
fi
echo ""

echo "=== Test Complete ==="
