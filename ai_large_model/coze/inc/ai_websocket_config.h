/**
 * @file        ai_websocket_config.h
 * @brief       WebSocket配置模块头文件
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该头文件定义了WebSocket连接和Coze AI服务的配置参数。
 *              包括连接信息（botId、token、地址等）、
 *              会话配置（voiceId、conversationId等）以及
 *              语音处理参数（speechRate、roleId等）。
 *              支持从配置文件加载和动态设置配置。
 */

#ifndef __AI_WEBSOCKET_CONFI_H__
#define __AI_WEBSOCKET_CONFI_H__

/**
 * @brief   标记未使用的参数
 * 
 * @details 消除编译器关于未使用参数的警告。
 */
#define UNUSED(x)   (void)(x)

/**
 * @brief   WebSocket配置结构体
 * 
 * @details 该结构体封装了与Coze AI服务通信所需的所有配置参数，
 *          包括连接信息和会话配置。
 */
typedef struct {
    // Coze WebSocket连接配置
    const char *botId;                   /**< Coze机器人ID */
    const  char *token;                  /**< Coze访问令牌 */
    const char *adress;                  /**< Coze服务地址 */
    const char *path;                    /**< Coze WebSocket路径 */
    int port;                            /**< Coze服务端口 */
    
    // Coze会话配置
    char *voiceId;                       /**< 语音ID */
    char *conversationId;                /**< 会话ID */
    char *uid;                           /**< 用户ID */
    int speechRate;                      /**< 语速 */
    int roleId;                          /**< 角色ID */
    char *parameters;                    /**< 附加参数 */
    char *voice_processing_config;       /**< 语音处理配置 */
    char *voice_print_config;            /**< 声纹识别配置 */
}quec_ai_config;

/**
 * @brief   获取当前配置
 * 
 * @details 该函数用于获取当前生效的WebSocket配置。
 * 
 * @return  quec_ai_config* 配置结构体指针
 */
quec_ai_config * quec_ai_get_config(void);

/**
 * @brief   从文件加载配置
 * 
 * @details 该函数用于从指定的配置文件加载WebSocket配置。
 * 
 * @param   filename    [in] 配置文件路径
 * @return  int 成功返回0，失败返回负值错误码
 *          - 0: 加载成功
 *          - -1: 文件打开失败
 *          - -2: 解析失败
 *          - -3: 格式错误
 */
int quec_ai_set_config(char *filename);

/**
 * @brief   确保配置文件存在
 * 
 * @details 该函数用于检查并确保指定路径的配置文件存在，
 *          如果不存在则创建默认配置文件。
 * 
 * @param   config_path [in] 配置文件路径
 * @return  int 成功返回0，失败返回负值错误码
 *          - 0: 成功
 *          - -1: 创建失败
 *          - -2: 权限不足
 */
int quec_ai_ensure_config_file(const char *config_path);

/**
 * @brief   清空消息缓冲区
 * 
 * @details 该函数用于清空WebSocket消息缓冲区，
 *          重置消息队列状态。
 */
void quec_ai_clearmsg();

// /**
//  * @brief   WebSocket互斥锁
//  * 
//  * @details 用于保护WebSocket操作的全局互斥锁，
//  *          确保多线程环境下的线程安全。
//  */
// extern pthread_mutex_t lws_mutx;


#endif
