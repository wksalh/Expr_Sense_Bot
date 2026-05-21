#!/bin/bash

#===============================================================================
# @file        start_car.sh
# @brief       整车启动脚本
# @author      Smart AI Robot
# @date        2026/02/03
# @version     v1.0
# @copyright   Copyright (c) 2026 Smart AI Robot
# @license     MIT License
# 
# @details     该脚本用于启动整车服务，包括ROS2、AI语音模块、机器人表情等。
#===============================================================================

# Start Car Service for QuecPi Smart Car
# Shell script for starting car service

# Get base directory - try environment variable first, then detect from script location
if [ -n "$QUECPI_BASE_DIR" ]; then
    BASE_DIR="$QUECPI_BASE_DIR"
else
    # Get script directory and find base directory
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    BASE_DIR="$(dirname "$SCRIPT_DIR")"
    # If script is in service_config/run, BASE_DIR should be parent of service_config
    if [ "$(basename "$BASE_DIR")" = "service_config" ]; then
        BASE_DIR="$(dirname "$BASE_DIR")"
    fi
    # If script is deployed to /usr/local/bin/quecpi/, try to find from common locations
    if [ ! -d "$BASE_DIR/service_config" ] && [ ! -d "$BASE_DIR/ros2_ws" ]; then
        # Try common installation paths
        if [ -d "/mnt/car_test/quecpi_smartcar/service_config" ]; then
            BASE_DIR="/mnt/car_test/quecpi_smartcar"
        elif [ -d "/mnt/quecpi_smartcar/service_config" ]; then
            BASE_DIR="/mnt/quecpi_smartcar"
        fi
    fi
fi

# Set paths relative to base directory
SERVICE_CONFIG_DIR="${BASE_DIR}/service_config"
RUN_DIR="${SERVICE_CONFIG_DIR}/run"
DIR_LOG="${RUN_DIR}/log"
# ROS2 workspace directory - try multiple locations
# Priority: 1) Same level as BASE_DIR, 2) Under BASE_DIR, 3) /mnt/ros2_ws
if [ -d "$(dirname "${BASE_DIR}")/ros2_ws" ]; then
    ROS2_WS_DIR="$(dirname "${BASE_DIR}")/ros2_ws"
elif [ -d "${BASE_DIR}/ros2_ws" ]; then
    ROS2_WS_DIR="${BASE_DIR}/ros2_ws"
elif [ -d "/mnt/ros2_ws" ]; then
    ROS2_WS_DIR="/mnt/ros2_ws"
else
    ROS2_WS_DIR="${BASE_DIR}/ros2_ws"  # Default fallback
fi

# AI_Talker directory - try multiple locations
# Priority: 1) Same level as BASE_DIR, 2) Under BASE_DIR, 3) /mnt/AI_Talker
if [ -d "$(dirname "${BASE_DIR}")/AI_Talker" ]; then
    AI_TALKER_DIR="$(dirname "${BASE_DIR}")/AI_Talker"
elif [ -d "${BASE_DIR}/AI_Talker" ]; then
    AI_TALKER_DIR="${BASE_DIR}/AI_Talker"
elif [ -d "/mnt/AI_Talker" ]; then
    AI_TALKER_DIR="/mnt/AI_Talker"
else
    AI_TALKER_DIR="${BASE_DIR}/AI_Talker"  # Default fallback
fi

# Emotion directory - try multiple locations
# Priority: 1) Same level as BASE_DIR, 2) Under BASE_DIR, 3) /mnt/emotion, 4) /mnt/car_test/emotion
if [ -d "$(dirname "${BASE_DIR}")/emotion" ]; then
    EMOTION_DIR="$(dirname "${BASE_DIR}")/emotion"
elif [ -d "${BASE_DIR}/emotion" ]; then
    EMOTION_DIR="${BASE_DIR}/emotion"
elif [ -d "/mnt/car_test/emotion" ]; then
    EMOTION_DIR="/mnt/car_test/emotion"
else
    EMOTION_DIR="${BASE_DIR}/emotion"  # Default fallback
fi

# Export environment variables for child processes
export QUECPI_BASE_DIR="$BASE_DIR"
export QUECPI_SERVICE_CONFIG_DIR="$SERVICE_CONFIG_DIR"
export QUECPI_RUN_DIR="$RUN_DIR"
export QUECPI_ROS2_WS_DIR="$ROS2_WS_DIR"
export QUECPI_AI_TALKER_DIR="$AI_TALKER_DIR"
export QUECPI_EMOTION_DIR="$EMOTION_DIR"

# 关键修改1：设置图形显示环境变量（针对Pygame/PySDL2）
# 由于系统没有X11，使用framebuffer驱动
export DISPLAY=:0
export SDL_VIDEODRIVER=fbcon       # 使用framebuffer控制台驱动
export SDL_FBDEV=/dev/fb0          # 指定framebuffer设备
export SDL_VIDEO_FBCON_ROTATION=0  # 不旋转屏幕
export PYGAME_BLEND_ALPHA_SDL2=1   # 启用SDL2混合
export PYGAME_SDL2_VIDEO=1         # 使用SDL2视频后端

# Ensure log directory exists
mkdir -p "$RUN_DIR" 2>/dev/null || true

# Set log files
LOG_FILE="${DIR_LOG}/start_car.log"
ROS2_LOG_FILE="${DIR_LOG}/ros2.log"
IOT_LOG_FILE="${DIR_LOG}/ai_car_demo.log"
WEBSOCKET_LOG_FILE="${DIR_LOG}/smart_car_demo.log"
ROBOT_FACE_LOG_FILE="${DIR_LOG}/robot_face.log"  # 新增：专门记录robot_face日志

# Log function
log() {
    # Ensure log directory exists before writing
    mkdir -p "$(dirname "$LOG_FILE")" 2>/dev/null || true
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOG_FILE" 2>/dev/null || echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1"
}

# 错误处理函数
error_exit() {
    log "Error: $1"
    exit 1
}

COZE_ENV_FILE="${COZE_ENV_FILE:-/etc/ai_car/coze.env}"
if [ -f "$COZE_ENV_FILE" ]; then
    set -a
    . "$COZE_ENV_FILE"
    set +a
    log "Loaded Coze credential environment from $COZE_ENV_FILE"
else
    log "Coze credential environment file not found: $COZE_ENV_FILE"
fi

log "=== Starting Car Service ==="

# 阶段 1：硬件初始化
log "Phase 1: Hardware initialization"

# 开机清除已保存的 WiFi 连接配置，避免配网失败仍误报「已连接」
NM_CONN_DIR="/etc/NetworkManager/system-connections"
if [ -d "$NM_CONN_DIR" ]; then
    log "Clearing saved WiFi connections: $NM_CONN_DIR"
    rm -f "$NM_CONN_DIR"/* 2>/dev/null || true
    log "Saved WiFi connections cleared"
else
    log "Warning: $NM_CONN_DIR not found, skipping"
fi

# tinymix 设置
log "Setting up audio mixer..."
if command -v tinymix &> /dev/null; then
    tinymix set "AUX_RDAC Switch" 1 || log "Warning: tinymix setup failed, continuing..."
else
    log "Warning: tinymix command not found, skipping audio mixer setup"
fi

# 启动 rgpiod
log "Starting GPIO daemon..."
rgpiod &
sleep 1

# rgs 命令序列
log "Configuring GPIO..."
rgs c 999 go 4 || error_exit "rgs go command failed"
rgs c 999 gso 0 142 || error_exit "rgs gso command failed"
rgs c 999 gw 0 142 1 || error_exit "rgs gw command failed"

# 设置音频输出
log "Setting audio output port..."
pactl set-sink-port 0 speaker || error_exit "Audio port setup failed"

# 在某些情况下扩展音量
tinymix set "RX_RX2 Digital Volume" 110
tinymix -D 1 set "RX_RX2 Digital Volume" 110

tinymix set "RX_RX0 Mix Digital Volume" 110
tinymix set "RX_RX1 Mix Digital Volume" 110
tinymix set "RX_RX2 Mix Digital Volume" 110

tinymix set "HPHL Volume" 20
tinymix set "HPHR Volume" 20

tinymix set "LO Switch" 1
tinymix set "AUX_RDAC Switch" 1

log "Hardware initialization completed"

# ===== Start v4l2loopback virtual camera service =====
log "Enabling and starting v4l2loopback service..."
systemctl daemon-reload 2>/dev/null || true
if ! systemctl is-enabled v4l2loopback.service &>/dev/null; then
    systemctl enable v4l2loopback.service || log "Warning: Failed to enable v4l2loopback.service"
fi
if ! systemctl is-active v4l2loopback.service &>/dev/null; then
    systemctl start v4l2loopback.service || log "Warning: Failed to start v4l2loopback.service"
else
    log "v4l2loopback.service is already running"
fi
# 可选：等待虚拟设备出现
sleep 1
log "v4l2loopback service status: $(systemctl is-active v4l2loopback.service)"

# 关键修改2：检查framebuffer设备是否存在
log "Checking framebuffer device..."
if [ -c /dev/fb0 ]; then
    log "Framebuffer device /dev/fb0 found"
    ls -l /dev/fb0 >> "$LOG_FILE"
else
    log "Warning: /dev/fb0 not found, trying alternative framebuffer driver"
    export SDL_VIDEODRIVER=dummy
fi

# Phase 2: ROS2 startup
# Phase 2: ROS2 startup
log "Phase 2: Starting ROS2"

# 快速设置 Weston 环境变量（并行运行）
(
    # 尝试获取 Weston 进程的环境变量
    weston_pid=$(pgrep -x weston 2>/dev/null)
    if [ -n "$weston_pid" ]; then
        # 从 Weston 进程环境读取
        if [ -f "/proc/$weston_pid/environ" ]; then
            xdg_runtime_dir=$(tr '\0' '\n' < "/proc/$weston_pid/environ" | grep ^XDG_RUNTIME_DIR= | cut -d= -f2)
            wayland_display=$(tr '\0' '\n' < "/proc/$weston_pid/environ" | grep ^WAYLAND_DISPLAY= | cut -d= -f2)
            if [ -n "$xdg_runtime_dir" ]; then
                export XDG_RUNTIME_DIR="$xdg_runtime_dir"
                [ -n "$wayland_display" ] && export WAYLAND_DISPLAY="$wayland_display"
            fi
        fi
    fi
) &

# 同时启动 robot_face.py（不等待环境变量完全设置）
ROBOT_FACE_LOG="${DIR_LOG}/robot_face.log"
ROBOT_FACE_PID=""

# 使用函数启动 robot_face，便于重试
start_robot_face() {
    local max_retries=3
    local retry_count=0
    
    while [ $retry_count -lt $max_retries ]; do
        if [ -f "${EMOTION_DIR}/robot_face.py" ]; then
            cd "${EMOTION_DIR}"
            nohup python3 robot_face.py >> "$ROBOT_FACE_LOG" 2>&1 &
            local pid=$!
            
            # 等待程序初始化
            sleep 1
            
            if kill -0 $pid 2>/dev/null; then
                echo $pid
                return 0
            fi
        fi
        
        retry_count=$((retry_count + 1))
        
        # 如果失败，等待一下再重试
        if [ $retry_count -lt $max_retries ]; then
            sleep 2
        fi
    done
    
    echo ""
    return 1
}

# 启动 robot_face.py
log "Starting robot_face.py..."
ROBOT_FACE_PID=$(start_robot_face)

if [ -n "$ROBOT_FACE_PID" ]; then
    log "robot_face.py started (PID: $ROBOT_FACE_PID)"
else
    log "Warning: robot_face.py failed to start after retries"
fi

# 确保必要的环境变量已设置
if [ -z "$XDG_RUNTIME_DIR" ]; then
    # 尝试常见路径
    for dir in "/run/user/1000" "/run/user/0" "/run/user/$(id -u)"; do
        if [ -d "$dir" ]; then
            export XDG_RUNTIME_DIR="$dir"
            break
        fi
    done
    [ -z "$XDG_RUNTIME_DIR" ] && export XDG_RUNTIME_DIR="/tmp"
fi

[ -z "$WAYLAND_DISPLAY" ] && export WAYLAND_DISPLAY="wayland-0"
[ -z "$DISPLAY" ] && export DISPLAY=":0"

# 继续原有流程（不等待 robot_face 完全初始化）
if [ ! -d "$ROS2_WS_DIR" ]; then
    log "Error: ROS2 workspace not found at $ROS2_WS_DIR"
    log "Checked locations: ${BASE_DIR}/ros2_ws, /mnt/ros2_ws"
    error_exit "Cannot find ROS2 workspace. Please ensure ros2_ws directory exists."
fi

cd "$ROS2_WS_DIR" || error_exit "Cannot switch to ROS2 workspace: $ROS2_WS_DIR"
log "Switched to ROS2 workspace: $ROS2_WS_DIR"
# Set environment variables and start ROS2
log "Starting ROS2 bringup..."
# Set environment variable and start ROS2 in background
# Add timestamp to each log line and output to both terminal and log file
bash -c "cd \"$ROS2_WS_DIR\" && export need_compile=True && source install/setup.bash && ros2 launch bringup bringup.launch.py" 2>&1 | while IFS= read -r line; do echo "[$(date '+%Y-%m-%d %H:%M:%S')] $line"; done | tee -a "$ROS2_LOG_FILE" &
ROS2_PID=$!
log "ROS2 bringup started (PID: $ROS2_PID), logs: $ROS2_LOG_FILE"



# 等待 ROS2 启动
sleep 3

# 阶段 3：AI 服务
log "Phase 3: AI services"

# 设置库路径并启动 AI 服务
log "Setting library path and starting AI services..."

sleep 10
#蓝牙配网操作
echo N > /sys/module/bluetooth/parameters/enable_ecred
systemctl restart bluetooth
sleep 1
hciconfig hci0 up
hciconfigssss=$(hciconfig)
log "Bluetooth hciconfig: $hciconfigssss"
sleep 1


# Start ai_car_demo with proper library path
log "Starting ai_car_demo..."
# Check if AI_Talker directory exists, try alternative locations if not
if [ ! -d "${AI_TALKER_DIR}/BIN" ]; then
    log "Warning: AI_Talker not found at ${AI_TALKER_DIR}"
    if [ -d "/mnt/AI_Talker/BIN" ]; then
        AI_TALKER_DIR="/mnt/AI_Talker"
        log "Using alternative AI_Talker directory: $AI_TALKER_DIR"
    fi
fi

sleep 5
if [ -f "${AI_TALKER_DIR}/BIN/ai_car_demo" ]; then
    # Use unbuffered output and higher priority for real-time audio processing
    (cd "${AI_TALKER_DIR}/BIN" && LD_LIBRARY_PATH="${AI_TALKER_DIR}/lib" nice -n -10 stdbuf -o0 -e0 "${AI_TALKER_DIR}/BIN/ai_car_demo" >> "$IOT_LOG_FILE" 2>&1) &
    IOT_PID=$!
    log "ai_car_demo started (PID: $IOT_PID), logs: $IOT_LOG_FILE"
else
    log "Warning: ai_car_demo not found at ${AI_TALKER_DIR}/BIN/ai_car_demo, skipping"
    IOT_PID=""
fi


log "=== All services started successfully ==="

# 信号处理函数
cleanup() {
    log "Received stop signal, shutting down all services..."
    
    # 终止robot_face.py进程
    if [ ! -z "$ROBOT_FACE_PID" ] && kill -0 $ROBOT_FACE_PID 2>/dev/null; then
        kill $ROBOT_FACE_PID 2>/dev/null
        log "Terminated robot_face.py process"
    fi
    
    # 终止所有后台进程
    if [ ! -z "$ROS2_PID" ]; then
        kill $ROS2_PID 2>/dev/null
        log "Terminated ROS2 process"
    fi
    
    if [ ! -z "$IOT_PID" ]; then
        kill $IOT_PID 2>/dev/null
        log "Terminated ai_car_demo process"
    fi
    
    # 终止 rgpiod
    pkill rgpiod 2>/dev/null
    log "Terminated rgpiod process"
    
    log "All services stopped"
    exit 0
}

# 注册信号处理器
trap cleanup SIGTERM SIGINT

log "Car service fully started, monitoring running status..."

# 监控所有进程
websocket_restart_count=0
max_restarts=3

while true; do
    # 检查 robot_face.py 进程
    if [ ! -z "$ROBOT_FACE_PID" ] && ! kill -0 $ROBOT_FACE_PID 2>/dev/null; then
        log "Warning: robot_face.py process has exited"
        # 可选：重启robot_face.py
        if [ -f "$ROBOT_FACE_PATH" ]; then
            log "Restarting robot_face.py..."
            cd "$ROBOT_FACE_DIR"
            python3 "$(basename "$ROBOT_FACE_PATH")" >> "$ROBOT_FACE_LOG_FILE" 2>&1 &
            ROBOT_FACE_PID=$!
            log "robot_face.py restarted (PID: $ROBOT_FACE_PID)"
        fi
    fi
    
    # 检查 ROS2 进程
    if [ ! -z "$ROS2_PID" ] && ! kill -0 $ROS2_PID 2>/dev/null; then
        log "Warning: ROS2 process has exited"
    fi
    
    # 检查 IOT 进程
    if [ ! -z "$IOT_PID" ] && ! kill -0 $IOT_PID 2>/dev/null; then
        log "Warning: ai_car_demo process has exited"
    fi
    
    if [ ! -z "$IOT_PID" ] && ! kill -0 $IOT_PID 2>/dev/null; then
        log "Warning: ai_car_demo process has exited"
        if [ $iot_restart_count -lt $max_restarts ]; then
            log "Restarting ai_car_demo..."
            ((iot_restart_count++))
            (cd "${AI_TALKER_DIR}/BIN" && LD_LIBRARY_PATH="${AI_TALKER_DIR}/lib" nice -n -10 stdbuf -o0 -e0 "${AI_TALKER_DIR}/BIN/ai_car_demo" >> "$IOT_LOG_FILE" 2>&1) &
            IOT_PID=$!
            log "ai_car_demo restarted (PID: $IOT_PID)"
        else
            log "Max restarts reached for ai_car_demo, giving up"
        fi
    fi
    
    sleep 10
done