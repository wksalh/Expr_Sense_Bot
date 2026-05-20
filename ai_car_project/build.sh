#!/bin/bash
source ../environment-setup-armv8-2a-qcom-linux

function build_all() {
    echo "=========================================="
    echo "开始编译可执行文件..."
    echo "=========================================="
    
    # 删除 build 目录（如果存在）
    if [ -d "build" ]; then
        echo "Deleting existing build directory..."
        rm -rf build
    fi
    
    # 创建 build 目录
    echo "Creating build directory..."
    mkdir build
    # 进入 build 目录
    cd build || exit
    # 执行 cmake 和 make
    echo "Running cmake..."
    cmake ..
    echo "Running make..."
    make
    cd ..
    echo "=========================================="
    echo "编译完成！"
    echo "=========================================="
}


function build_sdk()
{
    echo "=========================================="
    echo "开始编译 SDK..."
    echo "=========================================="
    
    
    TARGET_ROOT="./ai_car_output"
    clean_output() {
        echo "=========================================="
        echo "清理构建产物目录..."
        echo "=========================================="
        
        if [ -d "$TARGET_ROOT" ]; then
            rm -rf "$TARGET_ROOT"
            echo "已删除 $TARGET_ROOT 目录"
        else
            echo "$TARGET_ROOT 目录不存在，无需清理"
        fi
        
        echo "清理完成"
    }
    build_all
    # 创建目标目录结构
    echo "创建目标目录结构..."
    mkdir -p "$TARGET_ROOT/AI_Talker/BIN"
    mkdir -p "$TARGET_ROOT/AI_Talker/config"
    mkdir -p "$TARGET_ROOT/AI_Talker/lib"
    mkdir -p "$TARGET_ROOT/emotion"
    mkdir -p "$TARGET_ROOT/quecpi_smartcar/service_config/rules.d"
    mkdir -p "$TARGET_ROOT/quecpi_smartcar/service_config/run"
    mkdir -p "$TARGET_ROOT/quecpi_smartcar/service_config/service"
    mkdir -p "$TARGET_ROOT/ros2_ws/src"
    mkdir -p "$TARGET_ROOT/software/collect_picture"
    mkdir -p "$TARGET_ROOT/software/labelImg"
    mkdir -p "$TARGET_ROOT/software/lab_tool"
    mkdir -p "$TARGET_ROOT/software/Servo_upper_computer"

    cp ./build/ai_car_demo "$TARGET_ROOT/AI_Talker/BIN/ai_car_demo"
    rm -rf ./build
    if [ -d "./ai_audio/config" ]; then
        cp -r ./ai_audio/config/* "$TARGET_ROOT/AI_Talker/config/" 2>/dev/null || true
    else
        echo "ai_audio/config目录不存在,跳过相关文件拷贝"
    fi
    if [ -d "./ai_local_asr/quec_local_asr/config" ]; then
        cp -r ./ai_local_asr/quec_local_asr/config/* "$TARGET_ROOT/AI_Talker/config/" 2>/dev/null || true
    else
        echo "ai_local_asr/quec_local_asr/config目录不存在,跳过相关文件拷贝"
    fi
    if [ -d "./ai_large_model/coze/config" ]; then
        cp -r ./ai_large_model/coze/config/* "$TARGET_ROOT/AI_Talker/config/" 2>/dev/null || true
    else
        echo "ai_large_model/coze/config目录不存在,跳过相关文件拷贝"
    fi
    if [ -d "./ai_kws/quec_kws/config" ]; then
        cp -r ./ai_kws/quec_kws/config/* "$TARGET_ROOT/AI_Talker/config/" 2>/dev/null || true
    else
        echo "ai_kws/quec_kws/config目录不存在,跳过相关文件拷贝"
    fi
    if [ -d "ai_robot_face" ]; then
        cp ai_robot_face/*.gif "$TARGET_ROOT/emotion/" 2>/dev/null || true
        cp ai_robot_face/*.py "$TARGET_ROOT/emotion/" 2>/dev/null || true
        echo "  [OK] 表情文件已拷贝"
    else
        echo "  [WARN] ai_robot_face 目录未找到"
    fi
    cp -r "./ai_server/rules.d"/* "$TARGET_ROOT/quecpi_smartcar/service_config/rules.d/" 2>/dev/null || true
    cp -r "./ai_server/run"/* "$TARGET_ROOT/quecpi_smartcar/service_config/run/" 2>/dev/null || true
    cp -r "./ai_server/service"/* "$TARGET_ROOT/quecpi_smartcar/service_config/service/" 2>/dev/null || true

    if [ -d "./ai_ros2/ros2_ws/src" ]; then
        # 拷贝所有子目录，但排除构建产物目录
        for dir in ./ai_ros2/ros2_ws/src/*; do
            if [ -d "$dir" ]; then
                dirname=$(basename "$dir")
                # 排除 build、install、log 等构建产物目录
                if [ "$dirname" != "build" ] && [ "$dirname" != "install" ] && [ "$dirname" != "log" ]; then
                    cp -r "$dir" "$TARGET_ROOT/ros2_ws/src/" 2>/dev/null || true
                    echo "  [OK] ros2_ws/src/$dirname/"
                fi
            fi
        done
    else
        echo "  [WARN] ros2_ws 目录未找到，跳过"
    fi

    SOFTWARE_SRC="./ai_ros2//software"
    if [ -d "$SOFTWARE_SRC" ]; then
        for tool_dir in "$SOFTWARE_SRC"/*; do
            if [ -d "$tool_dir" ]; then
                tool_name=$(basename "$tool_dir")
                if [ "$(ls -A "$tool_dir" 2>/dev/null)" ]; then
                    cp -r "$tool_dir"/* "$TARGET_ROOT/software/$tool_name/" 2>/dev/null || true
                    echo "  [OK] $tool_name/"
                else
                    echo "  [INFO] $tool_name/ 需要手动配置"
                fi
            fi
        done
    else
        echo "  [WARN] $SOFTWARE_SRC 目录未找到"
    fi
    cp -rf ai_tracker_body "$TARGET_ROOT" 2>/dev/null || true
    #/*复制lib库*/
    cp -r ./ai_audio/lib/* "$TARGET_ROOT/AI_Talker/lib/" 2>/dev/null || true
    cp -r ./ai_kws/quec_kws/lib/* "$TARGET_ROOT/AI_Talker/lib/" 2>/dev/null || true
    cp -r ./ai_local_asr/quec_local_asr/lib/* "$TARGET_ROOT/AI_Talker/lib/" 2>/dev/null || true
    cp -r ./ai_qth/lib/* "$TARGET_ROOT/AI_Talker/lib/" 2>/dev/null || true
    echo "=========================================="
    echo "编译完成！"
    echo "=========================================="
}



