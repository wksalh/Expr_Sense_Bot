/**
 * @file        ai_audio_init.h
 * @brief       音频初始化模块头文件
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该头文件定义了音频系统初始化、唤醒词检测(KWS)初始化
 *              以及相关回调函数的接口。包含Websocket配置和音频处理
 *              相关的函数声明。
 */

#ifndef __AI_AUDIO_INIT_H__
#define __AI_AUDIO_INIT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "semaphore.h"
#include "stdbool.h"

/**
 * @brief   Websocket配置结构体
 * 
 * @details 该结构体用于存储Websocket相关的配置参数，
 *          包括音频保存选项和温度参数。
 */
typedef struct Websocket_Config_{
    float temperature;          /**< 降噪处理系数，用于音频处理配置 */
    int save_raw_audio;         /**< 是否保存原始音频数据，1-保存，0-不保存 */
    int save_aec_audio;         /**< 是否保存回声消除后的音频数据，1-保存，0-不保存 */
}Websocket_Config;

extern Websocket_Config* web_config;  /**< Websocket配置全局指针 */




/**
 * @brief   设置网络状态标志
 * 
 * @details 该函数用于设置网络连接状态标志，
 *          通知音频模块当前的网络连接情况。
 */
void ai_set_network_flag();

#ifdef __cplusplus
}
#endif

#endif
