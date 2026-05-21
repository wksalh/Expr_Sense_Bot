/**
 * @file        ai_common_define.h
 * @brief       公共定义头文件
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该头文件定义了模块管理器的核心类型定义，
 *              包括模块ID枚举、模块状态枚举等。
 *              所有子模块应包含此头文件以获取公共定义。
 */

#ifndef __AI_COMMON_DEFINE_H__
#define __AI_COMMON_DEFINE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief   模块ID枚举
 * 
 * @details 定义系统中所有模块的唯一标识符。
 *          每个模块对应一个唯一的ID，用于模块注册和查找。
 */
typedef enum {
    MODULE_ID_NONE = -1,       /**< 无效模块ID */
    MODULE_ID_MANAGER = 0,     /**< 模块管理器自身 */
    MODULE_ID_AUDIO=1,           /**< 音频模块 */
    MODULE_ID_QTH=2,             /**< 系统接口模块 */
    MODULE_ID_KWS=4,             /**< 唤醒词检测模块 */
    MODULE_ID_LOCAL_ASR=8,      /**< 本地ASR模块 */
    MODULE_ID_LARGE_MODEL=16,       /**< WebSocket模块 */
    MODULE_ID_COUNT            /**< 模块总数（用于数组大小） */
} module_id_t;

/**
 * @brief   模块状态枚举
 * 
 * @details 定义模块的生命周期状态。
 */
typedef enum {
    MODULE_STATE_UNINIT = 0,   /**< 未初始化 */
    MODULE_STATE_INIT,         /**< 已初始化 */
    MODULE_STATE_RUNNING,      /**< 运行中 */
    MODULE_STATE_ERROR,        /**< 错误状态 */
    MODULE_STATE_DEINIT        /**< 已销毁 */
} module_state_t;


// 前向声明 module_desc_t
typedef struct module_desc_s module_desc_t;

// 定义模块操作结构体
typedef struct module_ops {
    /**
     * @brief   初始化函数指针
     * @param   desc [in] 模块描述指针
     * @return  int 成功返回0，失败返回负值错误码
     */
    int (*init)(module_desc_t *desc);

    /**
     * @brief   销毁函数指针
     * @return  int 成功返回0，失败返回负值错误码
     */
    int (*destroy)(void);

    /**
     * @brief   设置参数/获取数据函数指针
     * @details 这是多功能函数，根据参数类型执行不同操作：
     *          - 传入NULL或特定结构体指针用于设置参数
     *          - 可以返回模块内部数据或状态
     *          - 具体实现由各模块自定义
     * @param   param   [in/out] 参数指针，由模块自定义
     * @return  int 成功返回0或正数，失败返回负值错误码
     */
    int (*set_param)(void *param);
} module_ops_t;

// 定义模块描述结构体
struct module_desc_s {
    module_id_t        id;             /**< 模块ID */
    const char        *name;           /**< 模块名称 */
    module_state_t     state;          /**< 当前状态 */
    module_ops_t      *ops;            /**< 模块ops指针 */
    struct module_desc_s *private_data; /**< 主模块指针 */
};

/*需公共层处理事件*/
#define KWS_EVENT_WAKEUP_RESULT      (MODULE_ID_MANAGER << 16 | 0x01)    /**< 唤醒词检测结果事件 */

/**
 * @brief   音频播放事件ID
 * 
 * @details 定义音频播放的事件ID，使用移位格式：module_id << 16 | event_code
 */
#define AUDIO_FILE_PLAY_EVENT     (MODULE_ID_AUDIO << 16 | 0x01)  /**< 本地音频播放事件 */
#define AUDIO_ONLIE_PLAY_EVENT    (MODULE_ID_AUDIO << 16 | 0x02)  /**< 在线音频流播放事件 */

/**
 * @brief   唤醒模块事件ID
 * 
 * @details 定义唤醒模块的事件ID，使用移位格式：module_id << 16 | event_code
 */
#define KWS_START_EVENT     (MODULE_ID_KWS << 16 | 0x01)  /**< 唤醒模块启动事件 */

/* 在线模块事件 */
#define LARGE_MODEL_START_EVENT     (MODULE_ID_LARGE_MODEL << 16 | 0x01)  /**< 在线模块启动事件 */
#define LARGE_MODEL_STOP_EVENT      (MODULE_ID_LARGE_MODEL << 16 | 0x02)  /**< 在线模块退出事件 */
#define LARGE_MODEL_START_INIT_EVENT  (MODULE_ID_LARGE_MODEL << 16 | 0x03)  /**< 在线模块正式初始化消息事件 */

/**
 * @brief   通用事件消息结构体
 * 
 * @details 用于模块间异步事件通信。
 */
typedef struct {
    uint32_t    event_id;      /**< 事件ID */
    void       *data;          /**< 事件数据 */
    int         data_len;       /**< 数据长度 */
    int         priority;       /**< 事件优先级（0=普通, 1=高优先级） */
    uint64_t    timestamp;      /**< 时间戳 */
} common_msg_t;


/**
 * @brief   事件队列节点
 * 
 * @details 事件队列链表节点。
 */
typedef struct event_node {
    common_msg_t msg;
    struct event_node *next;
} event_node_t;


common_msg_t* ai_common_create_msg(uint32_t event_id, void *data, int data_len, int priority);

void ai_common_free_msg(common_msg_t *msg);
/**
 * @brief   初始化服务定位器
 * 
 * @details 该函数用于初始化服务定位器，创建模块注册表。
 *          必须在所有模块操作之前调用。
 * 
 * @return  int 成功返回0，失败返回负值错误码
 */
int ai_common_service_init(void);

/**
 * @brief   销毁服务定位器
 * 
 * @details 该函数用于销毁服务定位器，释放所有资源。
 *          必须在所有模块销毁后调用。
 * 
 * @return  int 成功返回0，失败返回负值错误码
 */
int ai_common_service_deinit(void);

/**
 * @brief   注册模块
 * 
 * @details 该函数用于向服务定位器注册一个模块。
 *          模块应在初始化时调用此函数注册自己。
 * 
 * @param   id      [in] 模块ID
 * @param   name    [in] 模块名称
 * @param   ops     [in] 模块ops指针
 * @return  int 成功返回0，失败返回负值错误码
 */
int ai_common_register(module_id_t id, const char *name, void *ops);

/**
 * @brief   注销模块
 * 
 * @details 该函数用于从服务定位器注销一个模块。
 *          模块应在销毁前调用此函数注销自己。
 * 
 * @param   id      [in] 模块ID
 * @return  int 成功返回0，失败返回负值错误码
 * 
 */
int ai_common_unregister(module_id_t id);



/**
 * @brief   初始化所有模块
 * 
 * @details 该函数用于按依赖顺序初始化所有已注册的模块。
 * 
 * @return  int 成功返回0，失败返回负值错误码
 */
int ai_common_init_all(void);



/**
 * @brief   停止所有模块
 * 
 * @details 该函数用于停止所有运行中的模块。
 * 
 * @return  int 成功返回0，失败返回负值错误码
 */
int ai_common_stop_all(void);


#ifdef __cplusplus
}
#endif

#endif /* __AI_COMMON_DEFINE_H__ */

