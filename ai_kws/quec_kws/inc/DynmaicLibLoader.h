/**
 * @file        DynmaicLibLoader.h
 * @brief       动态库加载器头文件
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该头文件定义了动态库加载器类，用于跨平台加载动态链接库
 *              并获取其中的函数指针。提供了动态库加载、符号查找和释放功能。
 */

#pragma once
#ifndef __DYNMAICLIBLOADER_H__
#define __DYNMAICLIBLOADER_H__

/**
 * @brief   动态库句柄类型定义
 * 
 * @details 用于存储动态库加载句柄的通用类型。
 */
typedef void* DLL_HANDLE;

/**
 * @brief   动态库加载器类
 * 
 * @details 该类封装了动态库加载的跨平台实现，
 *          提供了加载库、获取函数地址、释放库等静态方法。
 */
class DynmaicLibLoader
{
public:
    /**
     * @brief   构造函数
     * 
     * @details 创建动态库加载器实例。
     */
    DynmaicLibLoader(void);

    /**
     * @brief   析构函数
     * 
     * @details 销毁动态库加载器实例。
     */
    ~DynmaicLibLoader(void);

public:
    /**
     * @brief   加载动态库
     * 
     * @details 该静态方法用于加载指定路径的动态库文件。
     *          支持Windows(.dll)和Linux(.so)平台的动态库。
     * 
     * @param   pLibFilePath  [in] 动态库文件的路径
     * @return  DLL_HANDLE 成功返回库句柄，失败返回NULL
     */
    static DLL_HANDLE loadLibrary(const char *pLibFilePath);

    /**
     * @brief   获取函数指针
     * 
     * @details 该静态方法用于从已加载的动态库中获取指定函数地址。
     * 
     * @param   hDll          [in] 动态库句柄
     * @param   pProcName     [in] 要获取的函数名称
     * @return  void* 成功返回函数指针，失败返回NULL
     */
    static void* getProcAddress(DLL_HANDLE hDll, const char *pProcName);

    /**
     * @brief   释放动态库
     * 
     * @details 该静态方法用于释放已加载的动态库，释放相关资源。
     * 
     * @param   hDll  [in] 要释放的动态库句柄
     */
    static void freeLibrary(DLL_HANDLE hDll);
};

#endif

