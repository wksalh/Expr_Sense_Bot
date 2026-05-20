/**
 * @file        ai_audio_play.h
 * @brief       音频播放模块头文件
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该头文件定义了音频播放相关的接口函数和结构体，
 *              包括音频数据播放、播放控制、上下文管理等。
 *              使用PulseAudio作为音频后端。
 */

#ifndef __AI_AUDIO_PALY_H__
#define __AI_AUDIO_PALY_H__


#ifdef __cplusplus
extern "C" {
#endif


#include <stdbool.h>
#include <pulse/simple.h>

/**
 * @brief   音频数据最大长度
 * 
 * @details 定义单次音频播放数据的最大字节数限制。
 */
#define     AUDIO_MAX_DATA_LEN      2048

/**
 * @brief   WiFi连接提示音文件路径
 */
#define     PLEASE_CONNECT_WIFI        "./../config/please_connect_wifi.wav"

/**
 * @brief   WiFi连接错误提示音文件路径
 */
#define     CONNECT_WIFI_ERROR         "./../config/connect_wifi_error.wav"

/**
 * @brief   WiFi连接成功提示音文件路径
 */
#define     CONNECT_WIFI_SUCCESS       "./../config/connect_success.wav"

/**
 * @brief   连接运行提示音文件路径
 */
#define     CONNECT_RUN                "./../config/connect_run.wav"

/**
 * @brief   播放音频数据
 * 
 * @details 该函数用于将音频数据送入播放队列进行播放，
 *          数据将被异步处理以保证播放的连续性。
 * 
 * @param   data        [in] 音频数据指针
 * @param   data_len    [in] 音频数据长度（字节数）
 * @return  int 成功返回0，失败返回负值错误码
 *          - 0: 播放请求成功
 *          - -1: 数据指针为空或数据长度不对
 */
int quec_ai_audio_play(char *data,int data_len);

/**
 * @brief   刷新音频播放缓冲区
 * 
 * @details 该函数用于清空当前播放缓冲区中的待播放数据，
 *          确保立即停止播放并清空队列。
 */
void quec_ai_audio_play_flush(void);

/**
 * @brief   打开音频播放流
 * 
 * @details 该函数用于创建并配置PulseAudio音频播放流，
 *          设置采样率、通道数、采样格式等参数。
 * 
 * @return  int 成功返回0，失败返回负值错误码
 *          - 0: 打开成功
 *          - -1: 创建流失败
 */
int quec_ai_audio_play_open_stream(void);

/**
 * @brief   初始化音频播放线程
 * 
 * @details 该函数用于启动音频播放后台线程，
 *          线程负责从播放队列中读取数据并送入音频流。
 * 
 * @return  int 成功返回0，失败返回负值错误码
 *          - 0: 初始化成功
 *          - -1: 创建流失败
 */
int quec_ai_audio_play_pthread_init(void);

/**
 * @brief   销毁音频播放线程
 * 
 * @details 该函数用于停止并销毁音频播放线程，
 *          释放相关资源。
 */
void quec_ai_audio_play_pthread_destroy(void);

/**
 * @brief   本地播放音频文件
 * 
 * @details 该函数用于直接播放指定的音频文件，
 *          适用于播放提示音、铃声等固定音频。
 * 
 * @param   filename [in] 音频文件路径
 */
void quec_ai_local_play_audio(char* filename);



#ifdef __cplusplus
}
#endif

#endif //__QUEC_AI_AUDIO_PALY_H__
