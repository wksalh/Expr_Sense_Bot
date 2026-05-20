#!/bin/bash

echo "=== 测试1920x1080 MJPEG配置 ==="
echo

# 1. 检查摄像头设备
echo "🔍 检查摄像头设备..."
if [ -L "/dev/depth_cam" ]; then
    CAMERA_DEVICE=$(readlink /dev/depth_cam)
    echo "✅ 找到摄像头软链接: /dev/depth_cam -> $CAMERA_DEVICE"
else
    echo "⚠️ 未找到摄像头软链接，使用 /dev/video0"
    CAMERA_DEVICE="/dev/video0"
fi

# 2. 检查摄像头是否支持1920x1080 MJPEG
echo "📊 检查摄像头支持的格式..."
echo "支持的格式和分辨率:"
v4l2-ctl --device "$CAMERA_DEVICE" --list-formats-ext 2>/dev/null | grep -A 20 "MJPEG"

# 3. 确保配置正确
echo "📝 确保配置正确..."
cat > $(dirname "$0")/usb_cam_param.yaml << EOF
/**:
    ros__parameters:
      video_device: "/dev/depth_cam"
      framerate: 30.0
      io_method: "mmap"
      frame_id: "camera"
      pixel_format: "mjpeg"
      av_device_format: "MJPEG"
      image_width: 1920
      image_height: 1080
      camera_name: "usb_cam"
      camera_info_url: "package://peripherals/config/camera_info.yaml"
      brightness: -1
      contrast: -1
      saturation: -1
      sharpness: -1
      gain: -1
      auto_white_balance: false  
      white_balance: 4000
      autoexposure: true  
      exposure: 100
      autofocus: false
      focus: -1
EOF

echo "✅ 配置已设置为1920x1080 MJPEG"

# 4. 设置环境变量
export need_compile=true

# 5. 测试启动
echo "🚀 启动摄像头节点..."
ros2 launch peripherals usb_cam.launch.py &
CAMERA_PID=$!

# 等待启动
sleep 5

# 检查是否启动成功
if kill -0 $CAMERA_PID 2>/dev/null; then
    echo "✅ 摄像头启动成功！"
    echo "进程ID: $CAMERA_PID"
    echo
    echo "配置信息:"
    echo "- 分辨率: 1920x1080"
    echo "- 格式: MJPEG"
    echo "- IO方法: mmap"
    echo "- 帧率: 30fps"
    echo
    echo "现在可以："
    echo "- 查看图像: ros2 run rqt_image_view rqt_image_view"
    echo "- 查看话题: ros2 topic echo /usb_cam/image_raw"
    echo "- 查看话题信息: ros2 topic info /usb_cam/image_raw"
    echo "- 停止摄像头: kill $CAMERA_PID"
    echo
    echo "按 Ctrl+C 停止摄像头"
    
    # 等待用户中断
    trap "echo '🛑 停止摄像头...'; kill $CAMERA_PID; exit" INT
    wait $CAMERA_PID
else
    echo "❌ 摄像头启动失败"
    echo "尝试使用read方法..."
    
    # 尝试使用read方法
    sed -i 's/io_method: "mmap"/io_method: "read"/' src/peripherals/config/usb_cam_param.yaml
    
    echo "🔄 使用read方法重新启动..."
    ros2 launch peripherals usb_cam.launch.py &
    CAMERA_PID=$!
    sleep 5
    
    if kill -0 $CAMERA_PID 2>/dev/null; then
        echo "✅ 使用read方法启动成功！"
        echo "进程ID: $CAMERA_PID"
        echo "按 Ctrl+C 停止摄像头"
        trap "echo '🛑 停止摄像头...'; kill $CAMERA_PID; exit" INT
        wait $CAMERA_PID
    else
        echo "❌ 两种方法都失败了"
        echo "可能的原因："
        echo "1. 摄像头不支持1920x1080 MJPEG格式"
        echo "2. 摄像头驱动问题"
        echo "3. 内存不足"
        echo
        echo "建议尝试较低分辨率："
        echo "  image_width: 1280"
        echo "  image_height: 720"
        exit 1
    fi
fi
