/**
 * @file        ai_local_asr_ops.c
 * @brief       Local ASR模块ops实现
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该文件实现了本地ASR模块的所有操作接口。
 *              采用三函数模式：init、destroy、set_param
 */

#include "ai_local_asr_ops.h"
#include "ai_local_asr.h"
#include "ai_common_define.h"
#include "ai_log.h"



static module_desc_t * local_asr_ops;

/**
 * @brief   Local ASR模块初始化
 */
static int local_asr_ops_init(module_desc_t * self_ops)
{
    quec_ai_log(LOG_AI_INFO,"Initializing Local ASR module...");
    local_asr_ops=self_ops;
    // int ret = Intelligent_Asr_Init(self_ops);
    // if (ret != 0) {
    //     quec_ai_log(LOG_AI_INFO,"Local ASR init failed: %d", ret);
    //     return ret;
    // }
    
    quec_ai_log(LOG_AI_INFO,"Local ASR module initialized successfully");
    return 0;
}

/**
 * @brief   Local ASR模块销毁
 */
static int local_asr_ops_destroy(void)
{
    quec_ai_log(LOG_AI_INFO,"Destroying Local ASR module...");
    stop_asr_deal();
    quec_ai_log(LOG_AI_INFO,"Local ASR module destroyed");
    return 0;
}

/**
 * @brief   Local ASR模块设置参数/获取数据
 * 
 * @details 这是多功能函数，根据参数执行不同操作：
 *          - param = NULL: 模块进入运行状态（相当于start）
 *          - param = 其他值: 设置参数或获取数据（由具体实现定义）
 * 
 * @param   param   [in/out] 参数指针
 * @return  int 成功返回0，失败返回负值错误码
 */
static int local_asr_ops_set_param(void *param)
{
    if (param == NULL) {
        /* 进入运行状态（相当于start） */
        quec_ai_log(LOG_AI_INFO,"Local ASR module starting (via set_param)");
        run_asr_deal();
        return 0;
    }
    
    /* TODO: 处理参数设置或数据获取 */
    quec_ai_log(LOG_AI_INFO,"Local ASR set_param called with custom param");
    
    return 0;
}

/**
 * @brief   Local ASR模块ops实例（三函数模式）
 */
static module_ops_t g_local_asr_ops = {
    .init = local_asr_ops_init,
    .destroy = local_asr_ops_destroy,
    .set_param = local_asr_ops_set_param,
};

/**
 * @brief   获取Local ASR模块ops指针
 */
module_ops_t *ai_local_asr_get_ops(void)
{
    return &g_local_asr_ops;
}

/**
 * @brief   注册Local ASR模块到服务定位器
 */
int ai_local_asr_ops_register(void)
{
    return ai_common_register(MODULE_ID_LOCAL_ASR, "LOCAL_ASR", &g_local_asr_ops);
}

