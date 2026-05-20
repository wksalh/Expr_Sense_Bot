#===============================================================================
# @file        CMakeLists.txt
# @brief       AI Car Demo 构建配置文件
# @author      Smart AI Robot
# @date        2026/02/03
# @version     v1.0
# @copyright   Copyright (c) 2026 Smart AI Robot
# @license     MIT License
# 
# @details     该文件定义了AI Car Demo项目的编译规则和依赖配置。
#===============================================================================

cmake_minimum_required(VERSION 3.10)
project(ai_car_demo)

# 编译选项：开启调试信息，禁用优化，启用严格警告
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -g3 -ggdb   -Wextra -Wall -fno-eliminate-unused-debug-types -fno-inline -std=c11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -g3 -ggdb   -Wextra -Wall -fno-eliminate-unused-debug-types -fno-inline -std=c++11")

# 设置 C 和 C++ 标准（可选，但推荐）
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # 禁用编译器扩展

# 头文件目录
include_directories(
    ai_audio/inc
    ai_kws/quec_kws/inc
    ai_kws/
    ai_local_asr/quec_local_asr/inc
    ai_local_asr/
    ai_qth/inc
    ai_audio/inc
    ai_large_model/coze/inc
    ai_large_model/
    ai_common/inc
    ../sysroots/armv8-2a-qcom-linux/usr/include/aarch64-linux-gnu
    ../sysroots/armv8-2a-qcom-linux/usr/include
    ./
)

# 源文件
file(GLOB SOURCES
    "ai_audio/src/*.c"
    "ai_kws/quec_kws/src/*.cpp"
    "ai_kws/*.c"
    "ai_local_asr/quec_local_asr/src/*.c"
    "ai_local_asr/*.c"
    "ai_qth/src/*.c"
    "ai_large_model/coze/src/*.c"
    "ai_large_model/*.c"
    "ai_common/src/*.c"
    "src/*.c"
    "src/kws_file/*.c"
    "src/kws_file/*.cpp"
    "*.c"
)

message(STATUS "Source files: ${SOURCES}")

link_directories(./ai_audio/lib)
link_directories(./ai_kws/lib)
link_directories(./ai_local_asr/quec_local_asr/lib)
link_directories(./ai_qth/lib)
link_directories(./ai_kws/lib)

# 生成可执行文件
add_executable(ai_car_demo ${SOURCES})

# 链接库
target_link_libraries(ai_car_demo
    crypto
    websockets
    cjson
    asound
    ssl
    pulse-simple
    pulse
    jansson
    curl
    pthread
    portaudio
    MNN
    QuecAEC_ANS
    aitalkone_shared
    fftw3
    qth_ble_wifi_1.0_
    liteav
    realtime_av_sender
    user_sig_gen
)

# 生成 Map 文件（可选）
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=output.map")
