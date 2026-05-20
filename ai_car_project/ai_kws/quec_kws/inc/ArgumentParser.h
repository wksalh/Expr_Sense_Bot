/**
 * @file        ArgumentParser.h
 * @brief       命令行参数解析器头文件
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该头文件定义了命令行参数解析器类，用于解析和存储
 *              命令行传入的参数。提供了参数解析、获取、打印等功能。
 */

#pragma once
#include <map>
#include <string>

/**
 * @brief   最大支持参数数量
 * 
 * @details 定义参数解析器支持的最大参数数量限制。
 */
#define MAX_AGR_COUNT (5)

/**
 * @brief   命令行参数解析器类
 * 
 * @details 该类用于解析命令行参数，将参数名称和值存储在映射表中，
 *          供后续模块获取使用。支持标准参数的预定义。
 */
class ArgumentParser
{
public:
    /**
     * @brief   构造函数
     * 
     * @details 创建参数解析器实例，初始化内部数据结构。
     */
    ArgumentParser();

    /**
     * @brief   析构函数
     * 
     * @details 销毁参数解析器实例，释放相关资源。
     */
    ~ArgumentParser();

public:
    /**
     * @brief   解析命令行参数字符串
     * 
     * @details 该函数用于解析命令行传入的参数数组，
     *          将参数名称和值存储到内部映射表中。
     * 
     * @param   nArgumentCount   [in] 参数数量
     * @param   pArgumentArray   [in] 参数数组（包含参数名和参数值）
     * @return  int 成功返回0，失败返回负值错误码
     *          - 0: 解析成功
     *          - -1: 参数数量超出限制
     *          - -2: 参数格式错误
     */
    int parseStr(int nArgumentCount, char **pArgumentArray);

    /**
     * @brief   获取参数值
     * 
     * @details 该函数用于根据参数名称获取对应的值。
     *          如果参数不存在，返回空字符串。
     * 
     * @param   szArgumentName   [in] 参数名称
     * @return  std::string 参数值字符串
     */
    std::string getParmaValue(std::string szArgumentName);

    /**
     * @brief   打印支持的参数列表
     * 
     * @details 该函数用于打印所有支持的参数名称和使用说明，
     *          方便用户了解可用的参数选项。
     */
    void printSupportArguments();

    /**
     * @brief   获取参数映射表
     * 
     * @details 该函数返回指向内部参数映射表的指针，
     *          供外部直接访问参数数据。
     * 
     * @return  std::map<std::string, std::string>* 参数映射表指针
     */
    std::map<std::string, std::string> *getParamMap();

public:
    /**
     * @brief   支持的参数名称数组
     * 
     * @details 预定义的支持参数名称列表，包含最多MAX_AGR_COUNT个参数。
     */
    const static std::string SUPPORT_ARG_NAMES[MAX_AGR_COUNT];

private:
    std::map<std::string, std::string> mapParams;  /**< 参数名称-值映射表 */
};
