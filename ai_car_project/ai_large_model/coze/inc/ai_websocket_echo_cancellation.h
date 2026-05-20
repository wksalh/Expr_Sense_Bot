/**
 * @file        ai_websocket_echo_cancellation.h
 * @brief       回声消除模块头文件
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该头文件定义了音频回声消除（AEC）的数据接口。
 *              提供了设置参考信号（播放音频）和采集信号（麦克风音频）的方法，
 *              以及获取处理后音频数据的功能。
 *              用于实现全双工语音通信中的回声消除功能。
 */

#ifndef __AI_WEBSOCKET_ECHO_CANCELLATION_H__
#define __AI_WEBSOCKET_ECHO_CANCELLATION_H__

/**
 * @brief   设置采集的音频数据
 * 
 * @details 该函数用于将麦克风采集的音频数据输入到回声消除模块，
 *          作为需要进行回声消除处理的原始信号。
 * 
 * @param   data    [in] 音频数据指针
 * @param   size    [in] 音频数据大小（字节数）
 */
void set_read_audio_data(char* data,int size);

/**
 * @brief   设置参考音频数据
 * 
 * @details 该函数用于将播放的音频数据输入到回声消除模块，
 *          作为计算回声参考的信号源。
 * 
 * @param   data    [in] 音频数据指针
 * @param   size    [in] 音频数据大小（字节数）
 */
void set_play_audio_data(char* data,int size);

/**
 * @brief   获取回声消除后的音频数据
 * 
 * @details 该函数用于获取经过回声消除处理后的音频数据，
 *          可直接用于后续的语音识别或传输。
 * 
 * @return  char* 回声消除后的音频数据指针，处理失败返回NULL
 */
char* get_echo_audio_data();

#endif
