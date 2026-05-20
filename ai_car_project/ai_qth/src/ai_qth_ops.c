/**
 * @file        ai_qth_ops.c
 * @brief       QTH模块ops实现
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该文件实现了QTH模块的所有操作接口。
 *              采用三函数模式：init、destroy、set_param
 */

#include "ai_qth_ops.h"
#include "ai_common_define.h"
#include "ai_log.h"
#include "qth_control.h"
#include "ai_common_shm.h"   

static module_desc_t *qth_module_desc = NULL;  // 用于保存Websocket模块的描述信息指针

void qthlib_callback(int action, int times, bool valid)
{
    quec_ai_log(LOG_AI_INFO,"qth callback event=%d, %d, %d\n", action, times, valid);
    if(action<QTH_ONLINE_EXIT)
    {
        send_ttlv_data_to_ros2(action, times, valid);  //小车动作直接传输到ros2执行
    }else{
        switch (action)
        {
            case QTH_ONLINE_EXIT:
                common_msg_t *msg1 = ai_common_create_msg(LARGE_MODEL_STOP_EVENT,NULL,0,1);
		        qth_module_desc->private_data->ops->set_param(msg1);
                /* code */
                break;
            case QTH_MAIX_MIC_CAR:
                break;
            case QTH_CONNECT_WIFI:
                common_msg_t *msg2 = ai_common_create_msg(LARGE_MODEL_START_INIT_EVENT,NULL,0,1);
		        qth_module_desc->private_data->ops->set_param(msg2);
                break;
            case QTH_CONNECT_WIFI_AUDIO:
                switch (times)
                {
                    case 1:
                        common_msg_t *msg3 = ai_common_create_msg(AUDIO_FILE_PLAY_EVENT,"./../config/connect_run.wav",strlen("./../config/connect_run.wav"),1);
		                qth_module_desc->private_data->ops->set_param(msg3);
                        break;
                    case 2:
                        common_msg_t *msg4 = ai_common_create_msg(AUDIO_FILE_PLAY_EVENT,"./../config/connect_success.wav",strlen("./../config/connect_success.wav"),1);
		                qth_module_desc->private_data->ops->set_param(msg4);
                        break;
                    case 3:
                        common_msg_t *msg5 = ai_common_create_msg(AUDIO_FILE_PLAY_EVENT,"./../config/connect_wifi_error.wav",strlen("./../config/connect_wifi_error.wav"),1);
		                qth_module_desc->private_data->ops->set_param(msg5);
                        break;
                    case 4:
                        common_msg_t *msg6 = ai_common_create_msg(AUDIO_FILE_PLAY_EVENT,"./../config/please_connect_wifi.wav",strlen("./../config/please_connect_wifi.wav"),1);
		                qth_module_desc->private_data->ops->set_param(msg6);
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }
}

/**
 * @brief   QTH模块初始化
 */
static int qth_ops_init(module_desc_t * self_ops)
{
    quec_ai_log(LOG_AI_INFO,"Initializing QTH module...");
    qth_module_desc=self_ops;
    int ret = qth_init(qthlib_callback);
    if (ret != 0) {
        quec_ai_log(LOG_AI_INFO,"QTH init failed: %d", ret);
        return ret;
    }
    
    quec_ai_log(LOG_AI_INFO,"QTH module initialized successfully");
    return 0;
}

/**
 * @brief   QTH模块销毁
 */
static int qth_ops_destroy(void)
{
    quec_ai_log(LOG_AI_INFO,"Destroying QTH module...");
    
    /* 销毁时停止设备 */
    Qth_devStop();
    Qth_devRemove();
    
    quec_ai_log(LOG_AI_INFO,"QTH module destroyed");
    return 0;
}

/**
 * @brief   QTH模块设置参数/获取数据
 * 
 * @details 这是多功能函数，根据参数执行不同操作：
 *          - param = NULL: 模块进入运行状态（相当于start）
 *          - param = 其他值: 设置参数或获取数据（由具体实现定义）
 * 
 * @param   param   [in/out] 参数指针
 * @return  int 成功返回0，失败返回负值错误码
 */
static int qth_ops_set_param(void *param)
{
    if (param == NULL) {
        /* 进入运行状态（相当于start） */
        quec_ai_log(LOG_AI_INFO,"QTH module starting (via set_param)");
        return Qth_devStart();
    }
    
    /* TODO: 处理参数设置或数据获取 */
    /* 这里可以扩展为处理不同的参数类型 */
    quec_ai_log(LOG_AI_INFO,"QTH set_param called with custom param");
    
    return 0;
}

/**
 * @brief   QTH模块ops实例（三函数模式）
 */
static module_ops_t g_qth_ops = {
    .init = qth_ops_init,
    .destroy = qth_ops_destroy,
    .set_param = qth_ops_set_param,
};

/**
 * @brief   获取QTH模块ops指针
 */
module_ops_t *ai_qth_get_ops(void)
{
    return &g_qth_ops;
}

/**
 * @brief   注册QTH模块到服务定位器
 */
int ai_qth_ops_register(void)
{
    return ai_common_register(MODULE_ID_QTH, "QTH", &g_qth_ops);
}

