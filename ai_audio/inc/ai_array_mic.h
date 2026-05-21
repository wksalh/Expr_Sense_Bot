/**
 * @file        ai_array_mic.h
 * @brief       麦克风阵列处理头文件
 * @author      Smart AI Robot
 * @date        2025/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该头文件定义了麦克风阵列初始化和混音处理的接口函数，
 *              主要用于音频采集和预处理功能。
 */

#ifndef _MAIX_MIC_ARRAY_H_
#define _MAIX_MIC_ARRAY_H_

/**
 * @brief   初始化麦克风阵列
 * 
 * @details 该函数用于初始化麦克风阵列设备，配置音频采集参数，
 *          建立与麦克风硬件的连接。初始化成功后即可开始音频采集。
 * 
 * @return  int 成功返回0，失败返回负值错误码
 *          - 0: 初始化成功
 *          - -1: 初始化失败
 */
int maix_mic_array_init();

/**
 * @brief   开启混音处理
 * 
 * @details 该函数用于是开启麦克风阵列的处理，
 */
void set_mixcar_deal();

#endif
