/**
 * *****************************************************************************
 * @file       : qlspeech_platform.h
 * @brief      : 语音前端平台配置头文件
 * @author     : lay.liu@quectel.com
 * @date       : 2024-05-06
 * @version    : v1.0
 * @copyright  : Copyright (c) 2024 Smart AI Robot
 * @license    : MIT License
 * @details    : 该头文件定义了语音前端库在不同平台下的配置选项。
 *              包括内存基本单元位数、字节序、指针对齐等基础配置，
 *              以及函数调用约定（stdcall、cdecl等）的平台适配。
 *              支持Windows和Linux双平台的条件编译配置。
 * *****************************************************************************
 */

/*----------------------------------------------+
 *  在这里包含目标平台程序需要的公共头文件
 +----------------------------------------------*/
/*
#include <tchar.h>
#include <assert.h>
#include <crtdbg.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
*/

/*----------------------------------------------+
 *  根据目标平台特性修改对应的配置选项
 +----------------------------------------------*/

#define QL_UNIT_BITS			8			        // 内存基本单元位数
#define QL_BIG_ENDIAN			0			        // 是否是 Big-Endian 字节序
#define QL_PTR_GRID				8			        // 最大指针对齐值

#define QL_PTR_PREFIX						        // 指针修饰关键字(典型取值有 near | far, 可以为空)
#define QL_CONST				const		        // 常量关键字(可以为空)
#define QL_EXTERN				extern		        // 外部关键字
#define QL_STATIC				static		        // 静态函数关键字(可以为空)

#if 0   // Windows
#define QL_INLINE				__inline	        // 内联关键字(典型取值有inline, 可以为空)
#define QL_CALL_STANDARD		__stdcall	        // 普通函数修饰关键字(典型取值有 stdcall | fastcall | pascal, 可以为空)
#define QL_CALL_REENTRANT		__stdcall	        // 递归函数修饰关键字(典型取值有 stdcall | reentrant, 可以为空)
#define QL_CALL_VAR_ARG			__cdecl		        // 变参函数修饰关键字(典型取值有 cdecl, 可以为空)
#endif

#if 1   // Linux
#define QL_INLINE				inline	            // 内联关键字(典型取值有 inline, 可以为空)
#define QL_CALL_STANDARD			                // 普通函数修饰关键字(典型取值有 stdcall | fastcall | pascal, 可以为空)
#define QL_CALL_REENTRANT			                // 递归函数修饰关键字(典型取值有 stdcall | reentrant, 可以为空)
#define QL_CALL_VAR_ARG					            // 变参函数修饰关键字(典型取值有 cdecl, 可以为空)
#endif

#define QL_TYPE_INT8			char		        // 8位数据类型
#define QL_TYPE_INT16			short		        // 16位数据类型
#define QL_TYPE_INT24			int			        // 24位数据类型
#define QL_TYPE_INT32			long		        // 32位数据类型

#if 0   // 48/64位数据类型是可选的, 如非必要则不要定义, 在某些32位平台下, 使用模拟方式提供的48/64位数据类型运算效率很低
#define QL_TYPE_INT48			__int64		        // 48位数据类型
#define QL_TYPE_INT64			__int64		        // 64位数据类型
#endif

#define QL_TYPE_ADDRESS			unsigned int		// 地址数据类型

#if 0   // Windows
#define QL_TYPE_SIZE			unsigned int		// 大小数据类型, unsigned int, unsigned long
#endif

#if 1   // Linux
#define QL_TYPE_SIZE			unsigned long		// 大小数据类型, unsigned int, unsigned long
#endif

#define QL_ANSI_MEMORY			0			        // 是否使用 ANSI 内存操作库
#define	QL_ANSI_STRING			0			        // 是否使用 ANSI 字符串操作库

#if 0
#define QL_ASSERT(exp)			assert(exp)         // 断言操作(可以为空)
#define QL_YIELD				Sleep(0)            // 空闲操作(在协作式调度系统中应定义为任务切换调用, 可以为空)
#endif

#if 1
#define QL_ASSERT(exp)			assert(exp)         // 断言操作(可以为空)
#define QL_YIELD				                    // 空闲操作(在协作式调度系统中应定义为任务切换调用, 可以为空)
#endif

/* 根据平台编译选项决定是否支持调试 */
#if defined(DEBUG) || defined(_DEBUG)
	#define QL_DEBUG			1			        // 是否支持调试
#else
	#define QL_DEBUG			0			        // 是否支持调试
#endif

/* 调试方式下则输出日志 */
#ifndef QL_LOG
	#define QL_LOG				QL_DEBUG	        // 是否输出日志
#endif

/* 根据平台编译选项决定是否以Unicode方式构建 */
#if defined(UNICODE) || defined(_UNICODE)
	#define QL_UNICODE			0			        // 是否以 Unicode 方式构建
#else
	#define QL_UNICODE			0			        // 是否以 Unicode 方式构建
#endif

#if defined(_WIN32)
    #include <windows.h>
    #define MAX_PATH_LEN MAX_PATH
#elif defined(__linux__) || defined(__APPLE__)
    #include <limits.h>
    #define MAX_PATH_LEN PATH_MAX
#endif