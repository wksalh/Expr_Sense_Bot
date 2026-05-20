/**
 * @file        ai_audio_ops.h
 * @brief       音频模块接口定义
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该头文件定义了音频模块的操作接口。
 *              采用三函数模式：init、destroy、set_param
 * 
 *              使用方法：
 *              1. 通过 ai_common_get_ops(MODULE_ID_AUDIO) 获取ops指针
 *              2. 调用 ops 中的函数指针访问音频功能
 */

#ifndef __AI_AUDIO_OPS_H__
#define __AI_AUDIO_OPS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "ai_common_define.h"


/**
 * @brief   获取音频模块ops指针
 * 
 * @details 该函数用于获取音频模块的ops指针。
 *          返回的指针可用于调用音频模块的所有功能。
 * 
 * @return  ai_audio_ops_t* 成功返回ops指针，失败返回NULL
 */
module_ops_t *ai_audio_get_ops(void);

/**
 * @brief   注册音频模块到服务定位器
 * 
 * @details 该函数用于将音频模块注册到服务定位器。
 *          通常在音频模块初始化时由模块自己调用。
 * 
 * @return  int 成功返回0，失败返回负值错误码
 */
int ai_audio_ops_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_AUDIO_OPS_H__ */

