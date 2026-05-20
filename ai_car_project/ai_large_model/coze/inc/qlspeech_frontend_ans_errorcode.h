/**
 * *****************************************************************************
 * @file       : qlspeech_frontend_ans_errorcode.h
 * @brief      : 语音前端ANS错误码定义头文件
 * @author     : lay.liu@quectel.com
 * @date       : 2024-07-22
 * @version    : v1.0
 * @copyright  : Copyright (c) 2024 Smart AI Robot
 * @license    : MIT License
 * @details    : 该头文件定义了语音前端ANS（降噪）库的所有错误码和状态码。
 *              包括通用错误码（参数错误、对象无效等）、ANS引擎错误码
 *              （VAD检测、降噪处理等）、授权系统错误码以及各子模块的
 *              特定错误码。用于统一错误处理和调试信息输出。
 * *****************************************************************************
 */

#ifndef QUECTEL_SPEECH_FRONTEND_ANS_LSA_ERRORCODE_H
#define QUECTEL_SPEECH_FRONTEND_ANS_LSA_ERRORCODE_H

#include "qlspeech_define.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef qlInt32	qlStatus;

// /* 状态码,向上累加,越详细越好 */
// #define QUEC_ANS_STATE_START_SPKING                 ((qlStatus) 1005)
// #define QUEC_ANS_STATE_STOP_SPKING                  ((qlStatus) 1004)
// #define QUEC_ANS_STATE_UNVOICED                     ((qlStatus) 1003)
// #define QUEC_ANS_STATE_VOICED                       ((qlStatus) 1002)
// #define QUEC_ANS_STATE_RUNNING                      ((qlStatus) 1001)

// #define QUEC_ANS_OK                                 ((qlStatus) 0)

// /* 错误码,向下递减,越详细越好 */
// #define QUEC_ANS_ERR                                ((qlStatus)-1001)
// #define QUEC_ANS_ERR_INVALID_ARG                    ((qlStatus)-1002)
// #define QUEC_ANS_ERR_INVALID_OBJ                    ((qlStatus)-1003)
// #define QUEC_ANS_ERR_INVALID_SRC                    ((qlStatus)-1004)
// #define QUEC_ANS_ERR_MEMORY					        ((qlStatus)-1005)
// #define QUEC_ANS_ERR_PARAM_TYPE                     ((qlStatus)-1006)
// #define QUEC_ANS_ERR_PARAM_VALUE                    ((qlStatus)-1007)
// #define QUEC_ANS_ERR_ENGINE_INIT                    ((qlStatus)-1008)
// #define QUEC_ANS_ERR_ENGINE_PROC                    ((qlStatus)-1009)

#define SLAS_OK 0
#define SLAS_NOK -1

typedef enum {
	QUEC_ANS_OK = 0,                            //正常返回
    //过渡期间兼容原错误码
    QUEC_ANS_ERR = -1001,
    QUEC_ANS_ERR_INVALID_ARG = -1002,
    QUEC_ANS_ERR_INVALID_OBJ = -1003,
    QUEC_ANS_ERR_INVALID_SRC = -1004,
    QUEC_ANS_ERR_MEMORY	= -1005,
    QUEC_ANS_ERR_PARAM_TYPE = -1006,
    QUEC_ANS_ERR_PARAM_VALUE = -1007,
    QUEC_ANS_ERR_ENGINE_INIT = -1008,
    QUEC_ANS_ERR_ENGINE_PROC = -1009,

    QUEC_ANS_STATE_START_SPKING = 1005,
    QUEC_ANS_STATE_STOP_SPKING = 1004,
    QUEC_ANS_STATE_UNVOICED = 1003,
    QUEC_ANS_STATE_VOICED = 1002,
    QUEC_ANS_STATE_RUNNING = 1001,

    QUEC_ANS_CREATE_ERR = 100001,               //创建引擎实例失败
	QUEC_ANS_INVALID_PARAM,                     //输入无效参数
	QUEC_ANS_INVALID_OBJ,                       //输入无效引擎实例
    QUEC_ANS_INVALID_FUNCTION,                  //该功能未开放
	QUEC_SLAS_TIMEOUT,                      //试用已超时
	QUEC_SLAS_INVALID_PARAM,                //授权系统输入无效参数
	QUEC_SLAS_PARAM_OVERLENGTH,             //授权系统输入参数字符串长度超出限制
	QUEC_SLAS_GET_DEVICE_INFO_ERR,          //授权系统获取设备信息失败
    QUEC_SLAS_BIO_ERR,                      //授权系统转换BIO失败
    QUEC_SLAS_RAS_ERR,                      //授权系统读取rsa失败
    QUEC_SLAS_ENCRYPT_DEVICE_INFO_ERR,      //授权系统加密设备信息失败
    QUEC_SLAS_BASE64_ERR,                   //授权系统base64编码失败
    QUEC_SLAS_CURL_ERR,                     //授权系统curl执行失败
    QUEC_SLAS_LICENSE_MALLOC_ERR,           //授权系统验证license申请内存失败
    QUEC_SLAS_LICENSE_OPEN_ERR,             //授权系统打开license文件失败
    QUEC_SLAS_LICENSE_READ_ERR,             //授权系统读取license文件失败
    QUEC_SLAS_LICENSE_VERIFY_ERR,           //授权系统验证该license与设备不匹配

    QUEC_ANS_VAD_SPEAK = 101001,                //ans vad检测有人生
    QUEC_ANS_VAD_NOSPEAK,                       //ans vad检测无人声
    QUEC_ANS_OMLSA_INIT_ERR,                    //omlsa初始化失败
    QUEC_ANS_OMLSA_RUNNING,                     //omlsa运行中
    QUEC_ANS_WIENER_INVALID_OBJ,                //wiener输入无效引擎实例
    QUEC_ANS_WIENER_INIT_ERR,                   //wiener初始化失败
    QUEC_ANS_WIENER_SAMPLE_SET_ERR,             //wiener采样率设置失败
    QUEC_ANS_WIENER_POLICY_SET_ERR,             //wiener设置策略失败
    QUEC_ANS_RNN_CREATE_ERR,                    //rnnoise创建实例失败
    QUEC_ANS_RNN_MODEL_READ_ERR,                //rnnoise模型读取失败
    QUEC_ANS_RNN_MODEL_INIT_ERR,                //rnnoise模型初始化失败
    QUEC_ANS_RNN_RUN_ERR,                       //rnnoise运行失败
    QUEC_ANS_RNN_INITED_ERR,                    //rnnoise已经初始化
    QUEC_ANS_AGC_CREATE_ERR,                    //agc创建实例失败
    QUEC_ANS_AGC_INIT_ERR,                      //agc初始化失败
    QUEC_ANS_AGC_MODE_INIT_ERR,                 //agc模式初始化失败
    QUEC_ANS_AGC_CONFIG_ERR,                    //agc配置失败
    QUEC_ANS_AGC_LEVEL_INIT_ERR,                //agc等级初始化失败
    QUEC_ANS_AGC_INVALID_OBJ,                   //agc输入无效句柄
    QUEC_ANS_AGC_INIT_INVALID_SAMPLE,           //agc初始化无效采样率
    QUEC_ANS_AGC_INPUT_SAMPLE_ERR,              //agc输入每帧数据长度与采样率不匹配
    QUEC_ANS_AGC_PROCESS_ERR,                   //agc处理失败
    QUEC_ANS_AGC_NOINIT,                        //agc未初始化
    QUEC_ANS_AGC_LIMITER_STATE_ERR,             //agc限幅器状态异常
    QUEC_ANS_AGC_LIMITER_LEVEL_ERR,             //agc限幅目标等级异常
    QUEC_ANS_AGC_CALC_GAIN_ERR,                 //agc计算增益失败
    QUEC_ANS_AGC_INVALID_PARAM,                 //agc输入无效参数
}QuecAnsErrCode_e;

#ifdef __cplusplus
}
#endif

#endif // QUECTEL_SPEECH_FRONTEND_ANS_LSA_ERRORCODE_H
