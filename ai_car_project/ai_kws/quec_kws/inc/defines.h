/**
 * @file        defines.h
 * @brief       通用定义和宏定义头文件
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该头文件定义了通用的宏定义、类型定义和错误检查辅助宏，
 *              用于KWS模块的跨平台开发和错误处理。
 */

#pragma once
#ifndef __DEFINE_HEADER__
#define __DEFINE_HEADER__

/**
 * @brief   函数返回值检查宏
 * 
 * @details 如果函数返回值不等于CODE_SUCCESS，则打印函数名并返回该返回值。
 *          用于简化错误处理流程。
 */
#define CHECK_FUNC_RET(ret,fun) if(ret != CODE_SUCCESS)\
{\
	printf("%s Failed!\n",#fun);\
	return ret;\
}

/**
 * @brief   API函数空指针检查宏
 * 
 * @details 检查API函数指针是否为空，如果为空则打印函数名并返回错误码。
 * 
 * @param   func    [in] 要检查的函数指针
 */
#define CHECK_API_NOT_NULL(func) if(func == NULL)\
{\
	printf("%s is NULL\n",#func);\
	return CODE_IVW_FUN_LOAD_FAIL;\
}


/**
 * @brief   IVW返回值检查宏
 * 
 * @details 如果IVW函数返回值不等于WIVW_SUCCESS，则打印函数名并返回该返回值。
 *          用于IVW引擎的错误处理。
 */
#define CHECK_IVW_RET(ret,fun) if(ret != WIVW_SUCCESS)\
{\
	printf("%s Failed!\n",#fun);\
	return ret;\
}

/**
 * @brief   指针检查宏
 * 
 * @details 检查指针是否存在或是否为空，根据参数条件判断并返回错误码。
 * 
 * @param   ptr      [in] 要检查的指针
 * @param   isExist  [in] 期望的指针存在状态（true-期望存在，false-期望为空）
 * @param   rlt      [in] 检查失败时的返回值
 * @param   msg      [in] 错误提示信息
 */
#define CHECK_PTR(ptr, isExist, rlt, msg) \
	if ((ptr != NULL) != isExist) \
{\
	printf(msg);\
	return rlt; \
}

/**
 * @brief   动态库句柄类型定义
 * 
 * @details 用于存储动态库加载句柄的通用类型。
 */
typedef void* DLL_HANDLE;

#ifdef WIN32
#include <sal.h>
#else
/**
 * @brief   Windows SAL注解（Linux平台占位）
 * 
 * @details 用于标记输入参数的注解，在Linux平台定义为空的宏。
 */
#define _In_
/**
 * @brief   Windows SAL注解可选参数（Linux平台占位）
 * 
 * @details 用于标记可选输入参数的注解，在Linux平台定义为空的宏。
 */
#define _In_opt_
/**
 * @brief   Windows SAL注解输出参数（Linux平台占位）
 * 
 * @details 用于标记输出参数的注解，在Linux平台定义为空的宏。
 */
#define _Out_
#endif /*WIN32*/

#endif

