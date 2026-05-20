#ifndef __AI_KWS_IVW_H__
#define __AI_KWS_IVW_H__


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   销毁唤醒词检测模块
 * 
 * @details 该函数用于释放KWS模块占用的所有资源，
 *          包括内存、模型数据和线程资源。
 */
int kws_destroy();

/**
 * @brief   KWS消息接收处理
 * 
 * @details 该函数用于接收和处理KWS模块的消息，
 *          包含唤醒词检测结果的解析和处理。
 */
void start_kws_deal();



#ifdef __cplusplus
}
#endif

#endif