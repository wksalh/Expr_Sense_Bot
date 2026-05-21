#!/bin/bash

echo "=== 摄像头问题诊断脚本 ==="
echo

# 检查设备状态
echo "1. 检查设备状态..."
ls -la /dev/video* /dev/depth_cam 2>/dev/null || echo "设备不存在"

echo
echo "2. 检查设备权限..."
ls -la /dev/video0 /dev/depth_cam 2>/dev/null | head -1

echo
echo "3. 测试v4l2-ctl基本功能..."
if v4l2-ctl --device=/dev/video0 --info >/dev/null 2>&1; then
    echo "✅ v4l2-ctl基本功能正常"
else
    echo "❌ v4l2-ctl基本功能失败"
fi

echo
echo "4. 测试摄像头格式..."
if v4l2-ctl --device=/dev/video0 --list-formats-ext >/dev/null 2>&1; then
    echo "✅ 摄像头格式检测正常"
    v4l2-ctl --device=/dev/video0 --list-formats-ext | head -10
else
    echo "❌ 摄像头格式检测失败"
fi

echo
echo "5. 测试摄像头流..."
echo "尝试启动摄像头流（5秒）..."
timeout 5s v4l2-ctl --device=/dev/video0 --stream-mmap --stream-count=1 --stream-to=/tmp/test_frame.raw >/dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "✅ 摄像头流测试成功"
    ls -la /tmp/test_frame.raw 2>/dev/null
    rm -f /tmp/test_frame.raw
else
    echo "❌ 摄像头流测试失败"
fi

echo
echo "6. 检查系统资源..."
echo "内存使用:"
free -h | head -2
echo
echo "CPU负载:"
uptime
echo
echo "磁盘空间:"
df -h / | head -2

echo
echo "7. 检查ROS2环境..."
if command -v ros2 >/dev/null 2>&1; then
    echo "✅ ROS2已安装"
    ros2 --version | head -1
else
    echo "❌ ROS2未安装"
fi

echo
echo "8. 检查usb_cam包..."
if ros2 pkg list | grep -q usb_cam; then
    echo "✅ usb_cam包已安装"
else
    echo "❌ usb_cam包未安装"
fi

echo
echo "=== 诊断完成 ==="
echo "如果以上测试都通过，问题可能在于："
echo "1. ROS2 usb_cam包的bug"
echo "2. 容器环境问题"
echo "3. 驱动兼容性问题"
echo "4. 内存不足"
