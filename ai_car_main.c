/**
 * @file        ai_car_main.c
 * @brief       AI Car主程序入口
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该文件实现了AI Car应用的主程序入口。
 *              使用服务定位器模式统一管理所有子模块。
 */

#include "ai_car_main.h"
#include "ai_log.h"
#include "ai_audio_ops.h"
#include "ai_kws_ops.h"
#include "ai_local_asr_ops.h"
#include "ai_qth_ops.h"
#include "ai_large_model_ops.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int signo)
{
    (void)signo;
    g_running = 0;
}

/* 日志宏简化定义 */


/**
 * @brief   注册所有模块ops到服务定位器
 */
static int register_all_modules(void)
{
    quec_ai_log(LOG_AI_INFO,"Registering all modules...");
    
    /* 注册QTH模块 */
    if (ai_qth_ops_register() != 0) {
        quec_ai_log(LOG_AI_INFO,"Failed to register QTH module");
        return -1;
    }
    
    /* 注册音频模块 */
    if (ai_audio_ops_register() != 0) {
        quec_ai_log(LOG_AI_INFO,"Failed to register AUDIO module");
        return -2;
    }
    
    /* 注册KWS模块 */
    if (ai_kws_ops_register() != 0) {
        quec_ai_log(LOG_AI_INFO,"Failed to register KWS module");
        return -3;
    }
    
    /* 注册Local ASR模块 */
    if (ai_local_asr_ops_register() != 0) {
        quec_ai_log(LOG_AI_INFO,"Failed to register LOCAL_ASR module");
        return -4;
    }
    
    /* 注册大模型模块 */
    if (ai_large_model_ops_register() != 0) {
        quec_ai_log(LOG_AI_INFO,"Failed to register WEBSOCKET module");
        return -5;
    }
    
    quec_ai_log(LOG_AI_INFO,"All modules registered successfully");
    return 0;
}

/**
 * @brief   程序主入口
 */
int main(void)
{
    int ret = 0;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    quec_ai_log(LOG_AI_INFO,"========================================");
    quec_ai_log(LOG_AI_INFO,"AI Car Demo Starting...");
    quec_ai_log(LOG_AI_INFO,"========================================");
    
    /* 1. 初始化服务定位器 */
    ret = ai_common_service_init();
    if (ret != 0) {
        quec_ai_log(LOG_AI_INFO,"Failed to initialize service: %d", ret);
        return -1;
    }
    quec_ai_log(LOG_AI_INFO,"Service initialized");
    
    /* 2. 注册所有模块ops */
    ret = register_all_modules();
    if (ret != 0) {
        quec_ai_log(LOG_AI_INFO,"Failed to register modules: %d", ret);
        ai_common_service_deinit();
        return -2;
    }
    
    /* 4. 初始化所有模块（按依赖顺序） */
    ret = ai_common_init_all();
    if (ret != 0) {
        quec_ai_log(LOG_AI_INFO,"Failed to init all modules: %d", ret);
        ai_common_service_deinit();
        return -3;
    }
    quec_ai_log(LOG_AI_INFO,"All modules initialized");
    
    quec_ai_log(LOG_AI_INFO,"========================================");
    quec_ai_log(LOG_AI_INFO,"AI Car Demo Started Successfully!");
    quec_ai_log(LOG_AI_INFO,"========================================");
    
    /* 8. 主循环 */
    while (g_running) {
        sleep(2);
        /* 可以在这里添加心跳日志或状态检查 */
        // quec_ai_log(LOG_AI_INFO,"AI Car running...");
    }
    
    /* 9. 停止所有模块 */
    ai_common_stop_all();
    
    /* 10. 销毁服务定位器 */
    ai_common_service_deinit();
    
    return 0;
}

