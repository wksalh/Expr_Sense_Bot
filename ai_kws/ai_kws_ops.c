/**
 * @file        ai_kws_ops.c
 * @brief       KWS模块ops实现
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该文件实现了KWS(唤醒词检测)模块的所有操作接口。
 *              采用三函数模式：init、destroy、set_param
 */

#include "ai_kws_ops.h"
#include "ai_common_define.h"
#include "quec_ai_kws_ivw.h"
#include "ai_log.h"
#include "ai_kws_ivw.h"


/**
 * @brief   KWS模块初始化
 */
static int kws_ops_init(module_desc_t * self_ops)
{
    quec_ai_log(LOG_AI_INFO,"Initializing KWS module...");
    int ret;
        /* 初始化KWS模型 */
    ret = kws_init(self_ops);
    if (ret != 0) {
        quec_ai_log(LOG_AI_INFO,"KWS init failed: %d", ret);
        return ret;
    }
    quec_ai_log(LOG_AI_INFO,"KWS module initialized successfully");
    return 0;
}

/**
 * @brief   KWS模块销毁
 */
static int kws_ops_destroy(void)
{
    quec_ai_log(LOG_AI_INFO,"Destroying KWS module...");
    /* TODO: 销毁KWS */
    quec_ai_log(LOG_AI_INFO,"KWS module destroyed");
    return 0;
}

/**
 * @brief   KWS模块设置参数/获取数据
 * 
 * @details 这是多功能函数，根据参数执行不同操作：
 *          - param = NULL: 模块进入运行状态（相当于start）
 *          - param = 其他值: 设置参数或获取数据（由具体实现定义）
 * 
 * @param   param   [in/out] 参数指针
 * @return  int 成功返回0，失败返回负值错误码
 */
static int kws_ops_set_param(void *param)
{
    int type=((common_msg_t*)param)->event_id;
    switch (type) {
        case KWS_START_EVENT:
            quec_ai_log(LOG_AI_INFO,"Received KWS_START_EVENT, starting kws processing");
            start_kws_deal();     
            break;
        default:
            break;
    }
    return 0;
}

/**
 * @brief   KWS模块ops实例（三函数模式）
 */
static module_ops_t g_kws_ops = {
    .init = kws_ops_init,
    .destroy = kws_ops_destroy,
    .set_param = kws_ops_set_param,
};

/**
 * @brief   获取KWS模块ops指针
 */
module_ops_t *ai_kws_get_ops(void)
{
    return &g_kws_ops;
}

/**
 * @brief   注册KWS模块到服务定位器
 */
int ai_kws_ops_register(void)
{
    return ai_common_register(MODULE_ID_KWS, "KWS", &g_kws_ops);
}

