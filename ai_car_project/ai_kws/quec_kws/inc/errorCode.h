/**
 * @file        errorCode.h
 * @brief       错误代码定义头文件
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该头文件定义了KWS模块使用的错误代码枚举类型，
 *              用于标识各种操作的成功或失败状态。
 */

#pragma once
#ifndef __ERROR_CODE_H__
#define __ERROR_CODE_H__

/**
 * @brief   错误代码枚举
 *
 * @details 定义了KWS模块可能返回的错误代码，
 *          包括成功状态和各种错误状态的编码。
 */
enum ErrorCode
{
CODE_SUCCESS = 0,/**< 成功 */
CODE_IVW_DLL_LOAD_FAIL = 100,/**< 加载dll动态库失败 */
CODE_IVW_FUN_LOAD_FAIL,/**< 加载接口函数失败 */
CODE_FILE_NOT_FOUND,/**< 文件未找到 */
CODE_FILE_LOAD_FAIL,/**< 文件加载失败，无法读取资源文件内容为空 */
CODE_FILE_OPEN_FAIL,/**< 文件打开失败，无法打开文件写入 */
CODE_FILE_EOF,/**< 文件读取结束 */
CODE_IVW_INST_RECREATE,/**< 唤醒实例重复创建 */
CODE_IVW_INST_NOT_EXIST,/**< 唤醒实例未创建 */
CODE_IVW_MANGER_RECREATE,/**< 唤醒管理器重复创建 */
CODE_IVW_MANGER_NOT_EXIST,/**< 唤醒管理器未创建 */
CODE_IVW_INST_NOT_START,/**< 唤醒实例未启动 */
CODE_PARAM_IS_NULL,/**< 参数为空 */
CODE_PARAM_ILEGAL/**< 参数不合法 */
};

#endif
