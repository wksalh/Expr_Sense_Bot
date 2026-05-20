/**
 * @file        ai_websocket.h
 * @brief       WebSocket通信模块头文件
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该头文件定义了WebSocket通信的核心接口。
 *              实现了与Coze AI服务的WebSocket连接管理，
 *              包括连接建立、数据收发、断开处理等功能。
 *              支持WiFi连接状态检测和自动重连机制。
 */

#ifndef __AI_WEBSOCKET_H__
#define __AI_WEBSOCKET_H__

#include "ai_websocket_config.h"
#include "ai_common_define.h"
#include <libwebsockets.h>
#include <stdbool.h>
#include <time.h>

/**
 * @brief   创建并启动WebSocket服务
 * 
 * @details 该函数用于创建libwebsockets服务实例，
 *          启动WebSocket服务器并开始监听连接。
 * 
 * @return  struct lws* 成功返回WebSocket服务指针，失败返回NULL
 */
struct lws * quec_ai_websocket(void);

/**
 * @brief   初始化WebSocket线程
 * 
 * @details 该函数用于启动WebSocket通信的后台线程，
 *          在独立线程中处理WebSocket事件循环。
 * 
 * @return  int 成功返回0，失败返回负值错误码
 *          - 0: 初始化成功
 *          - -1: 线程创建失败
 *          - -2: 内存分配失败
 */
int quec_ai_web_socket_pthread_init(module_desc_t *self_ops);

/**
 * @brief   断开WebSocket连接
 * 
 * @details 该函数用于主动断开当前WebSocket连接，
 *          释放相关资源并清理连接状态。
 * 
 * @return  int 成功返回0，失败返回负值错误码
 *          - 0: 断开成功
 *          - -1: 连接不存在
 */
int quec_ai_disconnect_websocket();

/**
 * @brief   检查WebSocket连接状态
 * 
 * @details 该函数用于查询当前WebSocket的连接状态，
 *          返回连接是否活跃。
 * 
 * @return  int 1-已连接，0-未连接
 */
int quec_ai_check_websocket_state(void);
void quec_ai_set_websocket_state(bool state);
bool websocket_get_state(void);
void quec_ai_set_online_asr_flag(bool state);
bool quec_ai_get_online_asr_flag(void);
void quec_ai_set_asr_connect_time(time_t connect_time);
time_t quec_ai_get_asr_connect_time(void);

/**
 * @brief   处理大模型开始事件
 * 
 * @details 该函数用于处理大模型的读写事件，
 *          包括接收Coze响应和发送用户请求。
 * 
 * @return  int 成功返回0，失败返回负值错误码
 *          - 0: 处理成功
 *          - -1: 连接断开
 *          - -2: 读写错误
 */
int start_large_model_deal();

/**
 * @brief   处理大模型开始初始化事件
 * 
 * @details 该函数用于处理大模型联网后正式初始化事件
 * 
 * @return  int 成功返回0，失败返回负值错误码
 *          - 0: 处理成功
 *          - -1: 连接断开
 *          - -2: 读写错误
 */
int start_init_large_mode();

/**
 * @brief   处理WiFi连接
 * 
 * @details 该函数用于处理WiFi连接状态变化，
 *          在WiFi断开时尝试重新连接。
 */
void connect_wifi_deal();

#endif
