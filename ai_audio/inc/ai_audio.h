/**
 * @file        ai_audio.h
 * @brief       音频处理核心头文件
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该头文件定义了音频处理的核心接口，包括唤醒词检测缓冲区、
 *              在线识别缓冲区、离线识别缓冲区、回调函数管理等。
 *              提供了完整的音频数据采集、处理和分发机制。
 */

#ifndef __AI_AUDIO_H__
#define AI_AUDIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <mqueue.h>
#include <pthread.h>


/**
 * @brief   唤醒词检测音频帧长度
 * 
 * @details 定义KWS模块每次处理的音频帧长度（采样点数）。
 */
#define  KWS_AUDIO_FRAME_LEN  320

/**
 * @brief   唤醒词检测音频帧数量
 * 
 * @details 定义KWS环形缓冲区容纳的音频帧数量。
 */
#define  KWS_AUDIO_FRAMES 10

/**
 * @brief   唤醒词检测环形缓冲区大小
 * 
 * @details KWS环形缓冲区的总字节大小。
 */
#define  KWS_RING_BUFFER_SIZE  KWS_AUDIO_FRAME_LEN*KWS_AUDIO_FRAMES

/**
 * @brief   在线识别音频帧长度
 * 
 * @details 在线ASR识别每次读取的音频长度（字节数）。
 *          计算公式：16位采样 * 2字节 * 30ms = 960字节
 */
#define  AUDIO_FRAME_LEN   1600

/**
 * @brief   在线识别每次读取长度
 * 
 * @details 在线识别每次从环形缓冲区读取的音频数据长度。
 */
#define  ONLINE_READ_LEN   640

/**
 * @brief   在线识别读取音频帧数量
 * 
 * @details 定义在线识别读取缓冲区的大小（帧数量）。
 */
#define  READ_AUDIO_FRAMES 3

/**
 * @brief   在线识别环形缓冲区大小
 * 
 * @details 在线识别环形缓冲区的总字节大小。
 */
#define  READ_RING_BUFFER_SIZE   ONLINE_READ_LEN*READ_AUDIO_FRAMES

/**
 * @brief   播放音频帧数量
 * 
 * @details 定义播放缓冲区容纳的音频帧数量。
 */
#define  PLAY_AUDIO_FRAMES 2400

/**
 * @brief   在线播放环形缓冲区大小
 * 
 * @details 在线播放环形缓冲区的总字节大小。
 *          可存储约120秒的音频数据(50ms*2400)。
 */
#define  PLAY_RING_BUFFER_SIZE   AUDIO_FRAME_LEN*PLAY_AUDIO_FRAMES

/**
 * @brief   在线识别循环时间
 * 
 * @details 在线识别处理的时间间隔计算值。
 */
#define  ONLINE_CYCLE_TIME ONLINE_READ_LEN*1000/16/4   

/**
 * @brief   离线识别采样率
 * 
 * @details 离线ASR识别的音频采样率（16kHz）。
 */
#define  ASR_SAMPLE_RATE 16000

/**
 * @brief   离线识别音频块大小
 * 
 * @details 离线识别每次处理的音频数据大小（字节数）。
 */
#define  OFFLINE_AUDIO_SIZE 4096

/**
 * @brief   回溯帧数
 * 
 * @details 离线识别需要回溯的音频帧数量，用于上下文关联。
 */
#define  LOOKBACK_FRAMES 5 

/**
 * @brief   离线识别环形缓冲区大小
 * 
 * @details 离线识别环形缓冲区的总字节大小。
 */
#define  ASR_RING_BUFFER_SIZE OFFLINE_AUDIO_SIZE*LOOKBACK_FRAMES

/**
 * @brief   每帧采样点数量
 * 
 * @details 计算每帧音频数据的采样点数量。
 */
#define  SAMPLES_PER_FRAME (OFFLINE_AUDIO_SIZE / sizeof(short))

/**
 * @brief   数据回调函数类型
 * 
 * @details 定义音频数据回调函数的类型，用于异步通知音频数据到达。
 * 
 * @param   data        [in] 音频数据指针
 * @param   data_len    [in] 音频数据长度（字节数）
 */
typedef void (*DataCallback)(void* data, int data_len);

void quec_ai_audio_register_data_callback(DataCallback callback);

/**
 * @brief   初始化音频系统
 * 
 * @details 该函数用于初始化整个音频系统，包括：
 *          - 初始化音频设备
 *          - 配置各类型缓冲区
 *          - 初始化回调函数列表
 *          - 启动音频读取线程
 * 
 * @return  int 成功返回0，失败返回负值错误码
 *          - 0: 初始化成功
 *          - -1: 初始化失败
 */
int quec_ai_audio_init();




#ifdef __cplusplus
}
#endif



#endif //__QUEC_AI_AUDIO_H__
