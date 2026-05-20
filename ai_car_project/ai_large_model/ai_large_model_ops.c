/**
 * @file        ai_large_model_ops.c
 * @brief       large_model模块ops实现
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该文件实现了large_model模块的所有操作接口。
 *              采用三函数模式：init、destroy、set_param
 */

#include "ai_common_define.h"
#include "ai_log.h"
#include "ai_large_model_ops.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*大模型私有头文件*/
#include "ai_websocket.h"

/**
 * @brief   large_model初始化标志
 */
static bool g_large_model_init_flag = false;

/**
 * @brief   large_model模块初始化
 */
static int large_model_ops_init(module_desc_t * self_ops)
{
    quec_ai_log(LOG_AI_INFO,"Initializing large_model module...\n");
    quec_ai_web_socket_pthread_init(self_ops);
    
    g_large_model_init_flag = true;
    
    quec_ai_log(LOG_AI_INFO,"large_model module initialized successfully\n");
    return 0;
}

/**
 * @brief   large_model模块销毁
 */
static int large_model_ops_destroy(void)
{
    quec_ai_log(LOG_AI_INFO,"Destroying large_model module...\n");
    
    // /* 断开连接 */
    // quec_ai_disconnect_large_model();
    
    g_large_model_init_flag = false;
    
    quec_ai_log(LOG_AI_INFO,"large_model module destroyed\n");
    return 0;
}

/**
 * @brief   large_model模块设置参数/获取数据
 * 
 * @details 这是多功能函数，根据参数执行不同操作：
 *          - param = NULL: 模块进入运行状态（相当于start）
 *          - param = 其他值: 设置参数或获取数据（由具体实现定义）
 * 
 * @param   param   [in/out] 参数指针
 * @return  int 成功返回0，失败返回负值错误码
 */
static int large_model_ops_set_param(void *param)
{
    int type=((common_msg_t*)param)->event_id;
    switch (type) {
        case LARGE_MODEL_START_EVENT:
            quec_ai_log(LOG_AI_INFO,"Received LARGE_MODEL_START_EVENT, starting large_model processing\n");
            start_large_model_deal(); 
            break;
        case LARGE_MODEL_STOP_EVENT:
            quec_ai_log(LOG_AI_INFO,"Received LARGE_MODEL_STOP_EVENT, stopping large_model processing\n");
             /* 断开连接 */
            quec_ai_disconnect_websocket();
            break;
        case LARGE_MODEL_START_INIT_EVENT:
            quec_ai_log(LOG_AI_INFO,"Received LARGE_MODEL_START_INIT_EVENT, large_model fully initialized\n");
            start_init_large_mode();
            start_large_model_deal(); 
            break;
        default:
            break;
    }
    return 0;
}

/**
 * @brief   large_model模块ops实例（三函数模式）
 */
static module_ops_t g_large_model_ops = {
    .init = large_model_ops_init,
    .destroy = large_model_ops_destroy,
    .set_param = large_model_ops_set_param,
};

/**
 * @brief   注册large_model模块到服务定位器
 */
int ai_large_model_ops_register(void)
{
    return ai_common_register(MODULE_ID_LARGE_MODEL, "large_model", &g_large_model_ops);
}

