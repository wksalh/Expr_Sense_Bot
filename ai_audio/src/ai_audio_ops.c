/**
 * @file        ai_audio_ops.c
 * @brief       音频模块ops实现
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该文件实现了音频模块的所有操作接口。
 *              采用三函数模式：init、destroy、set_param
 */

#include "ai_audio_ops.h"
#include "ai_audio.h"
#include "ai_audio_init.h"
#include "ai_array_mic.h"
#include "ai_common_define.h"
#include "ai_kws_ivw.h"
#include "ai_log.h"
#include "ai_audio_play.h"
#include <cjson/cJSON.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static module_desc_t * audio_ops;

/**
 * @brief   音频模块初始化
 */
static int audio_ops_init(module_desc_t * self_ops)
{
    quec_ai_log(LOG_AI_INFO,"Initializing audio module...");
    audio_ops=self_ops;
    /* 初始化音频系统 */
    int ret = quec_ai_audio_init();
    if (ret != 0) {
        quec_ai_log(LOG_AI_INFO,"Audio system init failed: %d", ret);
        return ret;
    }
    quec_ai_log(LOG_AI_INFO,"Audio module initialized successfully");
    return 0;
}

/**
 * @brief   音频模块销毁
 */
static int audio_ops_destroy(void)
{
    quec_ai_log(LOG_AI_INFO,"Destroying audio module...");
    
    /* 销毁KWS */
    kws_destroy();
    
    quec_ai_log(LOG_AI_INFO,"Audio module destroyed");
    return 0;
}

/**
 * @brief   音频模块设置参数/获取数据
 * 
 * @details 这是多功能函数，根据参数执行不同操作：
 *          - param = NULL: 模块进入运行状态（相当于start）
 *          - param = 其他值: 设置参数或获取数据（由具体实现定义）
 * 
 * @param   param   [in/out] 参数指针
 * @return  int 成功返回0，失败返回负值错误码
 */
static int audio_ops_set_param(void *param)
{
    if (param == NULL) {
        quec_ai_log(LOG_AI_INFO,"Audio set_param called with NULL param, starting audio processing");
        // start_audio_processing();
        return 0;
    }
    common_msg_t* msg=(common_msg_t*)param;
    quec_ai_log(LOG_AI_INFO,"Audio set_param called with custom param,id=%d,%s",msg->event_id,(char*)msg->data);
    switch (msg->event_id)
    {
    case AUDIO_FILE_PLAY_EVENT:   //播放音频文件
        if(msg->data!=NULL)
        {
            quec_ai_local_play_audio((char*)(msg->data));
        }
        break;
    case AUDIO_ONLIE_PLAY_EVENT:  // 在线播放音频数据
        quec_ai_audio_play((char*)(msg->data),msg->data_len);
        break;
    default:
        break;
    }
    return 0;
}

/**
 * @brief   音频模块ops实例
 */
static module_ops_t g_audio_ops = {
    .init = audio_ops_init,
    .destroy = audio_ops_destroy,
    .set_param = audio_ops_set_param,
};

/**
 * @brief   获取音频模块ops指针
 */
module_ops_t *ai_audio_get_ops(void)
{
    return &g_audio_ops;
}

/**
 * @brief   注册音频模块到服务定位器
 */
int ai_audio_ops_register(void)
{
    return ai_common_register(MODULE_ID_AUDIO, "AUDIO", &g_audio_ops);
}

