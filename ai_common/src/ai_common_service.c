/**
 * @file        ai_common_service.c
 * @brief       服务定位器实现文件
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该文件实现了服务定位器的所有功能，包括模块注册表管理、
 *              服务发现、模块生命周期管理等。
 *              采用单例模式，确保全局只有一个服务定位器实例。
 */

#include "ai_common_define.h"
#include "ai_common_message_queue.h"
#include "ai_common_shm.h"
#include "ai_log.h"
#include <cjson/cJSON.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

typedef struct common_config{
    uint32_t nework_flag;   /**  当前网络状态标志 */
} common_config_t;

common_config_t* g_common_config=NULL;

static MessageQueue manager_event_queue;

static pthread_t  g_event_dispatch_thread_id;
/**
 * @brief   模块注册表
 */
static module_desc_t g_module_registry[MODULE_ID_COUNT];

/**
 * @brief   服务定位器互斥锁
 */
static pthread_mutex_t g_service_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief   服务定位器是否已初始化
 */
static bool g_service_initialized = false;

/**
 * @brief   记录模块注册数量
 */
static int g_registered_count = 0;

/**
 * @brief   事件分发线程
 */
void *ai_common_event_dispatch_thread(void *arg)
{
    (void)arg;
    while (1) {
        common_msg_t *msg = (common_msg_t *)mq_pop(&manager_event_queue);
        if (msg == NULL) {
            continue;   
        }   
        quec_ai_log(LOG_AI_INFO,"Event dispatch thread received event: id=%d, priority=%d", msg->event_id, msg->priority);
        /* 根据event_id查找目标模块 */
        module_id_t target_module_id = (msg->event_id >> 16) & 0xFFFF;
        if (target_module_id <= MODULE_ID_NONE || target_module_id >= MODULE_ID_COUNT) {
            quec_ai_log(LOG_AI_INFO,"Invalid target module id extracted from event_id: %d", target_module_id);
            ai_common_free_msg(msg);
            continue;
        }   
        module_desc_t *target_module = &g_module_registry[target_module_id];
        if (target_module->ops == NULL || target_module->ops->set_param == NULL) {
            quec_ai_log(LOG_AI_INFO,"Target module %s does not have set_param function", target_module->name);
            ai_common_free_msg(msg);
            continue;
        }   
        /* 调用目标模块的set_param函数 */
        int ret = target_module->ops->set_param(msg);
        if (ret != 0) {
            quec_ai_log(LOG_AI_INFO,"Failed to handle event in module %s: %d", target_module->name, ret);
        } else {
            quec_ai_log(LOG_AI_INFO,"Event handled successfully by module %s,%p", target_module->name,target_module);
        }
        ai_common_free_msg(msg); 
    }
    return NULL;
}

static int manager_ops_init(module_desc_t * self_ops)
{
    mq_init(&manager_event_queue);
    if (init_shared_memory_ipc("/ros2_control_shm") != 0)
    {
       quec_ai_log(LOG_AI_INFO,"初始化共享内存失败\n");
       return -1;
    }
    /* 创建事件分发线程 */
    g_common_config=(common_config_t *)malloc(sizeof(common_config_t));
    if(g_common_config==NULL){
        quec_ai_log(LOG_AI_INFO,"Failed to allocate common_config_t for manager_ops_init");
        return -1;
    }
    g_common_config->nework_flag=false;
    int ret = pthread_create(&g_event_dispatch_thread_id, NULL, 
                             ai_common_event_dispatch_thread, self_ops);
    if (ret != 0) {
        quec_ai_log(LOG_AI_INFO,"Failed to create event dispatch thread: %d", ret);
        return -1;
    }
    quec_ai_log(LOG_AI_INFO,"Event queue initialized");
    return 0;
}

static int manager_ops_destroy(void)
{
    return 0;
}

static int manager_ops_set_param(void *param)
{

    common_msg_t *self_param=(common_msg_t *)param;
    if(self_param->event_id<(1<<16))
    {
        switch (self_param->event_id)
        {
        case KWS_EVENT_WAKEUP_RESULT:
            /* code */
            if(g_common_config!=NULL){
                if (!g_common_config->nework_flag)
                {
                    /* 开启在线识别 */
                    common_msg_t *msg = ai_common_create_msg(LARGE_MODEL_START_EVENT,NULL,0,1);
                    mq_push(&manager_event_queue, (char *)msg);
                }
            }
            break;
        default:
            break;
        }
    }else{
        mq_push(&manager_event_queue, (char *)param);
    }

    return 0;
}

/**
 * @brief   管理模块ops实例
 */
static module_ops_t g_manager_ops = {
    .init = manager_ops_init,
    .destroy = manager_ops_destroy,
    .set_param = manager_ops_set_param,
};

/**
 * @brief   注册所有模块
 * 
 * @details 在服务定位器初始化时注册所有已知模块。
 *          子模块也可以在初始化时自行注册。
 */
static void register_builtin_modules(void)
{
    /* 初始化模块注册表 */
    memset(g_module_registry, 0, sizeof(g_module_registry));
    /* 注册Manager模块 */
    g_module_registry[MODULE_ID_MANAGER].id = MODULE_ID_MANAGER;
    g_module_registry[MODULE_ID_MANAGER].name = "MANAGER";
    g_module_registry[MODULE_ID_MANAGER].state = MODULE_STATE_UNINIT;
    g_module_registry[MODULE_ID_MANAGER].ops = &g_manager_ops;
    g_registered_count = 1;
}

/**
 * @brief   初始化服务定位器
 */
int ai_common_service_init(void)
{
    pthread_mutex_lock(&g_service_mutex);
    
    if (g_service_initialized) {
        quec_ai_log(LOG_AI_INFO,"Service already initialized");
        pthread_mutex_unlock(&g_service_mutex);
        return 0;
    }
    
    quec_ai_log(LOG_AI_INFO,"Initializing common service...");
    
    /* 注册所有内置模块 */
    register_builtin_modules();
    
    g_service_initialized = true;
    
    quec_ai_log(LOG_AI_INFO,"Common service initialized");
    
    pthread_mutex_unlock(&g_service_mutex);
    return 0;
}

static void ai_common_deinit_all_locked(void)
{
    quec_ai_log(LOG_AI_INFO,"Deinitializing all modules (using destroy)...");

    /* 按逆序销毁模块 */
    for (int i = MODULE_ID_COUNT - 1; i >= 0; i--) {
        module_desc_t *mod = &g_module_registry[i];

        if (mod->ops == NULL || mod->state == MODULE_STATE_UNINIT) {
            continue;
        }
        module_ops_t *ops = (module_ops_t *)mod->ops;
        /* 使用destroy函数 */
        if (ops->destroy != NULL) {
            ops->destroy();
        }
        mod->state = MODULE_STATE_DEINIT;
        quec_ai_log(LOG_AI_INFO,"Module %s destroyed", mod->name);
    }

    quec_ai_log(LOG_AI_INFO,"All modules deinitialized");
}

/**
 * @brief   销毁所有模块
 *
 * @details 在三函数模式下，使用destroy函数销毁模块。
 */
int ai_common_deinit_all(void)
{
    pthread_mutex_lock(&g_service_mutex);
    ai_common_deinit_all_locked();
    pthread_mutex_unlock(&g_service_mutex);
    return 0;
}

/**
 * @brief   销毁服务定位器
 */
int ai_common_service_deinit(void)
{
    pthread_mutex_lock(&g_service_mutex);
    if (!g_service_initialized) {
        quec_ai_log(LOG_AI_INFO,"Service not initialized");
        pthread_mutex_unlock(&g_service_mutex);
        return 0;
    }
    quec_ai_log(LOG_AI_INFO,"Deinitializing common service...");
    /* 销毁所有模块 */
    ai_common_deinit_all_locked();
    g_service_initialized = false;
    g_registered_count = 0;
    quec_ai_log(LOG_AI_INFO,"Common service deinitialized");
    pthread_mutex_unlock(&g_service_mutex);
    return 0;
}

/**
 * @brief   注册模块
 */
int ai_common_register(module_id_t id, const char *name, void *ops)
{
    if (id <= MODULE_ID_NONE || id >= MODULE_ID_COUNT) {
        quec_ai_log(LOG_AI_INFO,"Invalid module id: %d", id);
        return -1;
    }
    
    if (name == NULL) {
        quec_ai_log(LOG_AI_INFO,"Module name cannot be NULL");
        return -2;
    }
    
    pthread_mutex_lock(&g_service_mutex);
    
    if (g_module_registry[id].ops != NULL) {
        quec_ai_log(LOG_AI_INFO,"Module %s already registered, overwriting", name);
    }
    
    g_module_registry[id].id = id;
    g_module_registry[id].name = name;
    g_module_registry[id].ops = ops;
    g_module_registry[id].state = MODULE_STATE_UNINIT;
    
    /* 更新注册计数（跳过Manager） */
    if (id != MODULE_ID_MANAGER && id >= g_registered_count) {
        g_registered_count = id + 1;
    }
    
    quec_ai_log(LOG_AI_INFO,"Module registered: %s (id=%d)", name, id);
    
    pthread_mutex_unlock(&g_service_mutex);
    return 0;
}

/**
 * @brief   注销模块
 */
int ai_common_unregister(module_id_t id)
{
    if (id <= MODULE_ID_NONE || id >= MODULE_ID_COUNT) {
        return -1;
    }
    
    pthread_mutex_lock(&g_service_mutex);
    
    if (g_module_registry[id].ops == NULL) {
        pthread_mutex_unlock(&g_service_mutex);
        return 0;
    }
    
    quec_ai_log(LOG_AI_INFO,"Module unregistered: %s", g_module_registry[id].name);
    
    memset(&g_module_registry[id], 0, sizeof(module_desc_t));
    g_module_registry[id].state = MODULE_STATE_UNINIT;
    
    pthread_mutex_unlock(&g_service_mutex);
    return 0;
}

/**
 * @brief   初始化所有模块
 */
int ai_common_init_all(void)
{
    quec_ai_log(LOG_AI_INFO,"Initializing all modules...");
    module_desc_t *commot_ops=NULL;
    pthread_mutex_lock(&g_service_mutex);
    
    /* 按依赖顺序初始化模块 */
    for (int i = MODULE_ID_MANAGER; i < MODULE_ID_COUNT; i++) {
        module_desc_t *mod = &g_module_registry[i];
        if(i==MODULE_ID_MANAGER){
            commot_ops=mod;
            mod->private_data=mod;
        }else{
            mod->private_data=commot_ops;
        }
        quec_ai_log(LOG_AI_INFO,"Module %s initialized", mod->name);
        /* 初始化模块 */
        if (mod->ops != NULL) {
            module_ops_t *ops = mod->ops;
            if (ops->init != NULL) {
                int ret = ops->init(mod);
                if (ret != 0) {
                    quec_ai_log(LOG_AI_INFO,"Failed to init %s: %d", mod->name, ret);
                    pthread_mutex_unlock(&g_service_mutex);
                    return ret;
                }
            }
        }
        mod->state = MODULE_STATE_INIT; 
    }
    pthread_mutex_unlock(&g_service_mutex);
    quec_ai_log(LOG_AI_INFO,"All modules initialized");
    return 0;
}

/**
 * @brief   停止所有模块
 * 
 * @details 在三函数模式下，stop功能通过set_param实现。
 *          这里通过set_param(NULL)通知模块进入停止状态。
 */
int ai_common_stop_all(void)
{
    quec_ai_log(LOG_AI_INFO,"Stopping all modules (using set_param)...");
    
    pthread_mutex_lock(&g_service_mutex);
    
    for (int i = MODULE_ID_COUNT - 1; i >= 0; i--) {
        module_desc_t *mod = &g_module_registry[i];
        
        if (mod->ops == NULL || mod->state != MODULE_STATE_RUNNING) {
            continue;
        }
        
        module_ops_t *ops = (module_ops_t *)mod->ops;
        /* 使用set_param(NULL)作为stop信号 */
        if (ops->set_param != NULL) {
            ops->set_param(NULL);
        }
        mod->state = MODULE_STATE_INIT;
        quec_ai_log(LOG_AI_INFO,"Module %s stopped (via set_param)", mod->name);
    }
    
    pthread_mutex_unlock(&g_service_mutex);
    quec_ai_log(LOG_AI_INFO,"All modules stopped");
    return 0;
}

/**
 * @brief   打印所有模块状态
 */
void ai_common_print_status(void)
{
    quec_ai_log(LOG_AI_INFO,"========== Module Status ==========");
    
    for (int i = 0; i < MODULE_ID_COUNT; i++) {
        module_desc_t *mod = &g_module_registry[i];
        const char *state_str[] = {"UNINIT", "INIT", "RUNNING", "ERROR", "DEINIT"};
        const char *reg_str = (mod->ops != NULL) ? "REG" : "---";
        
        quec_ai_log(LOG_AI_INFO,"  [%s] %s: %s", reg_str, mod->name,
             (mod->state >= 0 && mod->state <= 4) ? state_str[mod->state] : "UNKNOWN");
    }
    
    quec_ai_log(LOG_AI_INFO,"===================================");
}

/**
 * @brief   获取当前时间戳
 */
static uint64_t ai_common_get_timestamp(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}


common_msg_t* ai_common_create_msg(uint32_t event_id, void *data, int data_len, int priority)
{
    common_msg_t *msg = (common_msg_t *)malloc(sizeof(common_msg_t));
    if (msg == NULL) {
        quec_ai_log(LOG_AI_INFO,"Failed to allocate common_msg_t");
        return NULL;
    }
    
    msg->event_id = event_id;
    msg->priority = priority;
    msg->timestamp = ai_common_get_timestamp();
    
    if (data != NULL && data_len > 0) {
        msg->data = malloc(data_len+1); // +1 for null terminator
        memset(msg->data, 0, data_len+1); // 初始化内存，避免未定义行为
        if (msg->data == NULL) {
            quec_ai_log(LOG_AI_INFO,"Failed to allocate msg data");
            free(msg);
            return NULL;
        }
        memcpy(msg->data, data, data_len);
        msg->data_len = data_len;
    } else {
        msg->data = NULL;
        msg->data_len = 0;
    }
    
    return msg;
}


void ai_common_free_msg(common_msg_t *msg)
{
    if (msg == NULL) return;
    
    if (msg->data != NULL) {
        free(msg->data);
        msg->data = NULL;
    }
    
    free(msg);
}

