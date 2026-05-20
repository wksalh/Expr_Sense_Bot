/**
 * @file        ai_qth_ops.h
 * @brief       QTH模块接口定义
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该头文件定义了QTH系统接口模块的操作接口。
 *              采用三函数模式：init、destroy、set_param
 * 
 *              使用方法：
 *              1. 通过 ai_common_get_ops(MODULE_ID_QTH) 获取ops指针
 *              2. 调用 ops 中的函数指针访问QTH功能
 */

#ifndef __AI_QTH_OPS_H__
#define __AI_QTH_OPS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "ai_common_define.h"


/**
 * @brief   获取QTH模块ops指针
 * 
 * @details 该函数用于获取QTH模块的ops指针。
 *          返回的指针可用于调用QTH模块的所有功能。
 * 
 * @return  module_ops_t* 成功返回ops指针，失败返回NULL
 */
module_ops_t *ai_qth_get_ops(void);

/**
 * @brief   注册QTH模块到服务定位器
 * 
 * @details 该函数用于将QTH模块注册到服务定位器。
 *          通常在QTH模块初始化时由模块自己调用。
 * 
 * @return  int 成功返回0，失败返回负值错误码
 */
int ai_qth_ops_register(void);

#ifdef __cplusplus
}
#endif

#endif /* __AI_QTH_OPS_H__ */

