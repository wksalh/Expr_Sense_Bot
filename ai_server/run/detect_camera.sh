#!/bin/bash

echo "=== 摄像头设备检测和软链接工具 ==="
echo

# 检查v4l2-ctl是否可用
if ! command -v v4l2-ctl &> /dev/null; then
    echo "v4l2-ctl 命令不可用，请安装 v4l-utils"
    echo "sudo apt install v4l-utils"
    exit 1
fi

# 检查udevadm是否可用
if ! command -v udevadm &> /dev/null; then
    echo "udevadm 命令不可用"
    exit 1
fi

# 设置设备权限
echo "🔧 设置设备权限..."
chmod 666 /dev/video* 2>/dev/null || true

# 检测摄像头设备
echo "🔍 检测摄像头设备..."
camera_devices=()

for i in {0..8..2}; do
    device="/dev/video$i"
    
    if [ -e "$device" ]; then
        echo "📹 发现设备: $device"
        
        # 获取设备信息
        if v4l2-ctl --device "$device" --info >/dev/null 2>&1; then
            device_info=$(v4l2-ctl --device "$device" --info 2>/dev/null | head -5)
            echo "   设备信息: $device_info"
            
            # 检查是否为摄像头设备
            if echo "$device_info" | grep -i -E "(camera|webcam|usb|video|capture)" >/dev/null; then
                # 获取USB总线信息
                bus_info=$(udevadm info --query=property --name="$device" 2>/dev/null | grep "KERNELS=" | cut -d'=' -f2)
                
                camera_devices+=("$device:$bus_info")
                echo "✅ $device: 确认为摄像头设备"
                if [ -n "$bus_info" ]; then
                    echo "   USB总线: $bus_info"
                fi
            else
                echo "$device: 非摄像头设备"
            fi
        else
            echo "$device: 无法获取设备信息"
        fi
    fi
done

echo

if [ ${#camera_devices[@]} -eq 0 ]; then
    echo "未找到摄像头设备"
    echo "请检查:"
    echo "1. 摄像头是否正确连接"
    echo "2. 摄像头驱动是否安装"
    echo "3. 运行: lsusb 查看USB设备"
    exit 1
fi

echo "📊 找到 ${#camera_devices[@]} 个摄像头设备:"
for i in "${!camera_devices[@]}"; do
    device_info="${camera_devices[$i]}"
    device=$(echo "$device_info" | cut -d':' -f1)
    bus_info=$(echo "$device_info" | cut -d':' -f2)
    
    echo "  $((i+1)). $device"
    if [ -n "$bus_info" ]; then
        echo "     USB总线: $bus_info"
    fi
done

echo

# 测试设备可用性
echo "🧪 测试设备可用性..."
working_devices=()

for device_info in "${camera_devices[@]}"; do
    device=$(echo "$device_info" | cut -d':' -f1)
    
    if v4l2-ctl --device "$device" --list-formats-ext >/dev/null 2>&1; then
        echo "✅ $device: 设备可用"
        working_devices+=("$device_info")
    else
        echo "$device: 设备不可用"
    fi
done

echo

if [ ${#working_devices[@]} -eq 0 ]; then
    echo "没有可用的摄像头设备"
    exit 1
fi

# 选择主要设备（优先选择video0）
primary_device=""
for device_info in "${working_devices[@]}"; do
    device=$(echo "$device_info" | cut -d':' -f1)
    if [ "$device" = "/dev/video0" ]; then
        primary_device="$device_info"
        break
    fi
done

if [ -z "$primary_device" ]; then
    primary_device="${working_devices[0]}"
fi

selected_device=$(echo "$primary_device" | cut -d':' -f1)
echo "🎯 选择主要设备: $selected_device"

# 移除已存在的软链接
if [ -L "/dev/depth_cam" ]; then
    echo "移除已存在的软链接: /dev/depth_cam"
    rm -f /dev/depth_cam
fi

# 创建软链接
echo "创建软链接: $selected_device -> /dev/depth_cam"
ln -sf "$selected_device" /dev/depth_cam

if [ -L "/dev/depth_cam" ]; then
    target=$(readlink /dev/depth_cam)
    echo "软链接创建成功: /dev/depth_cam -> $target"
else
    echo "软链接创建失败"
    exit 1
fi

echo

# 更新udev规则
echo "更新udev规则..."
rules_file="/etc/udev/rules.d/99-usb-cam.rules"

# 生成新规则
new_rules=""
for device_info in "${working_devices[@]}"; do
    device=$(echo "$device_info" | cut -d':' -f1)
    bus_info=$(echo "$device_info" | cut -d':' -f2)
    device_name=$(basename "$device")
    
    if [ -n "$bus_info" ]; then
        rule="KERNEL==\"$device_name\", KERNELS==\"$bus_info\", MODE:=\"0777\", SYMLINK+=\"depth_cam\""
    else
        rule="KERNEL==\"$device_name\", MODE:=\"0777\", SYMLINK+=\"depth_cam\""
    fi
    
    new_rules="$new_rules$rule"$'\n'
done

# 写入规则文件
echo "$new_rules" > "$rules_file"
echo "更新udev规则文件: $rules_file"
echo "新规则:"
echo "$new_rules"

# 重新加载udev规则
echo "重新加载udev规则..."
udevadm control --reload-rules
udevadm trigger

echo
echo "摄像头设备检测和软链接完成！"
echo "主要设备: $selected_device"
echo "软链接: /dev/depth_cam -> $selected_device"
echo
echo "现在可以:"
echo "- 使用 /dev/depth_cam 访问摄像头"
echo "- 重启系统后软链接会自动创建"
echo "- 运行: ls -la /dev/depth_cam 查看软链接"
