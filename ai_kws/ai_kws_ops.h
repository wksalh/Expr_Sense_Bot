/**
 * @file        ai_kws_ops.h
 * @brief       KWS模块接口定义
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该头文件定义了唤醒词检测(KWS)模块的操作接口。
 *              采用三函数模式：init、destroy、set_param
 * 
 *              使用方法：
 *              1. 通过 ai_common_get_ops(MODULE_ID_KWS) 获取ops指针
 *              2. 调用 ops 中的函数指针访问KWS功能
 */

#ifndef __AI_KWS_OPS_H__
#define __AI_KWS_OPS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "ai_common_define.h"

/**
 * @brief   回调函数类型定义
 * 
 * @details 无参数无返回值的回调函数类型，用于音频事件通知。
 */
typedef void (*CallbackFuncs)(void);

/**
 * @brief   测试回调函数
 * 
 * @details 该函数是唤醒回调函数，用于接收唤醒事件信息。
 */
void myCallbackFunction();

/**
 * @brief   初始化唤醒词检测模块
 * 
 * @details 该函数用于初始化KWS(Keyword Spotting)唤醒词检测系统，
 *          加载模型并配置音频回调函数。初始化成功后，KWS模块将
 *          开始监听指定的唤醒词。
 * 
 * @param   callback   [in] 唤醒检测到时的回调函数指针
 */
int kws_init(module_desc_t * self_ops);

/**
 * @brief   获取KWS模块ops指针
 * 
 * @details 该函数用于获取KWS模块的ops指针。
 *          返回的指针可用于调用KWS模块的所有功能。
 * 
 * @return  ai_kws_ops_t* 成功返回ops指针，失败返回NULL
 */
module_ops_t *ai_kws_get_ops(void);

/**
 * @brief   注册KWS模块到服务定位器
 * 
 * @details 该函数用于将KWS模块注册到服务定位器。
 *          通常在KWS模块初始化时由模块自己调用。
 * 
 * @return  int 成功返回0，失败返回负值错误码
 */
int ai_kws_ops_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_KWS_OPS_H__ */

