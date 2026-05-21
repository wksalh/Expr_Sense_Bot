/**
 * @file        ai_websocket_coze.h
 * @brief       Coze AI事件处理模块头文件
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该头文件定义了Coze AI服务的事件处理接口。
 *              实现了Coze WebSocket消息的上下行事件分发机制，
 *              支持事件类型映射和自定义处理函数注册。
 */

#ifndef __AI_WEBSOCKET_COZE_H__
#define __AI_WEBSOCKET_COZE_H__
#include <stdint.h>
#include <string.h>
#include <cjson/cJSON.h>
#include <libwebsockets.h>

/**
 * @brief   Coze下行事件处理函数类型
 * 
 * @details 定义从Coze服务器接收数据的处理函数类型。
 * 
 * @param   wsi     [in] WebSocket实例指针
 * @param   in      [in] JSON解析后的数据对象
 * @param   len     [in] 数据长度
 * @return  int 处理结果，0表示成功
 */
typedef int (*quec_ai_coze_handler_downstream_t) (struct lws *wsi,cJSON *in,size_t len);

/**
 * @brief   Coze上行事件处理函数类型
 * 
 * @details 定义向Coze服务器发送数据的处理函数类型。
 * 
 * @param   wsi         [in] WebSocket实例指针
 * @param   event_type  [in] 事件类型字符串
 * @param   data        [in] 要发送的数据
 * @param   data_len    [in] 数据长度
 * @return  int 处理结果，0表示成功
 */
typedef int (*quec_ai_coze_handler_upstream_t)   (struct lws *wsi,char *event_type,char *data,int data_len);

/**
 * @brief   Coze下行事件映射表项
 * 
 * @details 定义下行事件类型与处理函数的映射关系。
 */
typedef struct quec_ai_coze_handle_downstream_map_t{
    char                                event_type[128];   /**< 事件类型名称 */
    quec_ai_coze_handler_downstream_t  func;               /**< 对应处理函数 */
}quec_ai_coze_handle_downstream_map;

/**
 * @brief   Coze上行事件映射表项
 * 
 * @details 定义上行事件类型与处理函数的映射关系。
 */
typedef struct quec_ai_coze_handle_upstream_map_t{
    char                                event_type[128];   /**< 事件类型名称 */
    quec_ai_coze_handler_upstream_t     func;               /**< 对应处理函数 */
}quec_ai_coze_handle_upstream_map;

/**
 * @brief   处理Coze上行事件
 * 
 * @details 该函数用于处理向Coze服务器发送的事件，
 *          根据事件类型调用相应的处理函数。
 * 
 * @param   wsi         [in] WebSocket实例指针
 * @param   event_type  [in] 事件类型字符串
 * @param   data        [in] 要发送的数据
 * @param   data_len    [in] 数据长度
 * @return  int 成功返回0，失败返回负值错误码
 */
int quec_ai_coze_handler_upstream_event(struct lws *wsi,char *event_type,char *data,int data_len);

/**
 * @brief   处理Coze下行事件
 * 
 * @details 该函数用于处理从Coze服务器接收的事件，
 *          根据事件类型分发到对应的处理函数。
 * 
 * @param   wsi         [in] WebSocket实例指针
 * @param   json_str    [in] 接收的JSON字符串
 * @param   len         [in] 字符串长度
 * @return  int 成功返回0，失败返回负值错误码
 */
int quec_ai_coze_handle_downstream_event(struct lws *wsi,char *json_str,size_t len);

/**
 * @brief   初始化Coze Token
 * 
 * @details 该函数用于获取并设置Coze访问令牌，
 *          为后续的WebSocket通信做准备。
 * 
 * @return  int 成功返回0，失败返回负值错误码
 */
int quec_ai_coze_token_init();

/**
 * @brief   Coze连接信息数据结构
 * 
 * @details 该结构体存储与Coze服务建立连接所需的信息。
 */
typedef struct {
    char *token;                 /**< Coze访问令牌 */
    char *botId;                 /**< 机器人ID */
    char *voiceId;               /**< 语音ID */
    char *conversationId;        /**< 会话ID */
    char *uid;                   /**< 用户ID */
    int speechRate;              /**< 语速 */
    char *parameters;            /**< 附加参数 */
    char *hotWords;              /**< 热词配置 */
    char *interruptConfig;       /**< 中断配置 */
    char *preAnswerList;         /**< 预回答列表 */
    int roleId;                  /**< 角色ID */
    char *voiceProcessingConfig; /**< 语音处理配置 */
    char *voicePrint;            /**< 声纹配置 */
} coze_connect_data_t;

/**
 * @brief   Coze响应数据结构
 * 
 * @details 该结构体用于解析Coze服务器的响应数据。
 */
typedef struct {
    int code;                    /**< 响应状态码 */
    char *msg;                   /**< 响应消息 */
    coze_connect_data_t *data;   /**< 连接信息数据 */
} coze_response_t;

#endif
