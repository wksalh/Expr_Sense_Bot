/**
 * *****************************************************************************
 * @file       : qlspeech_frontend_aec_ans_api.h
 * @brief      : 语音前端AEC+ANS降噪算法API头文件
 * @author     : rambo.zhang@quectel.com
 * @date       : 2024-12-02
 * @version    : v1.0
 * @copyright  : Copyright (c) 2024 Smart AI Robot
 * @license    : MIT License
 * @details    : 该头文件定义了语音前端处理库的AEC（回声消除）和ANS（降噪）
 *              算法的API接口。提供了创建句柄、初始化、处理音频帧、获取版本、
 *              设置参数和销毁等完整功能。支持单声道和双声道音频处理，
 *              可配置降噪强度和重置功能。
 * *****************************************************************************
 */

#ifndef QUECTEL_SPEECH_FRONTEND_ANS_AEC_ANS_API_H
#define QUECTEL_SPEECH_FRONTEND_ANS_AEC_ANS_API_H

#include "qlspeech_frontend_ans_errorcode.h"
#include "qlspeech_frontend_ans_quecans_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief      : 创建aec_ans句柄实例
     *
     * @param      : mode_path      aec_ans的模型地址，如果为NULL，则使用默认地址：宏 QUEC_ANS_MODEL_PATH_AEC_ANS 指向的 ./models/Exp002M138_8k_simple.mnn
     * @return     : qlPointer      aec_ans句柄，如果创建失败，则返回NULL
     */
    qlPointer qlCall aec_ans_create(
        char *mode_path // mode path, if NULL, use default value, mode path default value is "./models/xx.onnx" or "./models/xx.mnn"
    );

    /**
     * @brief      : aec_ans句柄的初始化
     *
     * @param      : pobj           aec_ans句柄实例
     * @return     : qlStatus       返回处理状态
     */
    qlStatus qlCall aec_ans_init(
        qlPointer pObj // point to the aec_ans object
    );

    /**
     * @brief      : aec_ans的处理操作
     *
     * @param      : pobj           aec_ans句柄实例
     * @param      : pInData        一次处理的输入数据_主mic
     * @param      : pInLpbData     一次处理的输入数据_参考mic
     * @param      : len            输入数据的采样点数
     * @param      : pOutData       一次处理的输出数据
     * @param      : pOutDataLen    输出数据的采样点数，仔细看，这个参数是指针，意味着每次输出数据的采样点数可能会和输入数据的采样点数不一样，当且仅当输入数据的采样点数为ANS_QUECANS_FRAME_LEN的整数倍时，输出数据的采样点数才和输入一样。
     * @return     : qlStatus       返回处理状态
     */
    qlStatus qlCall aec_ans_process(
        qlPointer pObj,       // point to the aec_ans object
        qlPInt16 pInData,     // [In]  input frame data for main mic
        qlPInt16 pInLpbData,  // [In]  input frame data for reference mic
        qlUInt32 len,         // [In]  input frame len,length of samples
        qlPInt16 pOutData,    // [Out] output frame data
        qlPUInt32 pOutDataLen // [Out] output frame len
    );

        /**
     * @brief      : aec_ans的处理操作
     *
     * @param      : pobj           aec_ans句柄实例
     * @param      : pInData        一次处理的输入数据，2ch 16bit 排列顺序 mic1 ref1 mic2 ref2 mic3 ref3 mic4 ref4
     * @param      : len            输入一帧的长度，也就是一帧2ch的总的字节数，比如：16k 16bit 2ch 一帧长度是640
     * @param      : rete           采样率，目前只支持16000Hz
     * @param      : inchannel      输入信号的通道数，目前只支持2
     * @param      : outchannel     输出信号的通道数，可以选择1或者2
     * @param      : pOutData       一次处理的输出信号
     * @param      : pOutDataLen    输出一帧的长度,也就是一帧outchannel的总的字节数，仔细看，这个参数是指针，意味着每次输出数据的字节数可能会和输入数据的字节数不一样，当且仅当输入数据的字节点数为640的整数倍时，输出数据的字节数才和输入一样。
     * @return     : qlStatus       返回处理状态
     */
    qlStatus qlCall aec_ans_process_2ch(
        qlPointer pObj,       // point to the aec_ans object
        qlPInt16 pInData,     // [In]  input frame data , must be 2 channel
        qlUInt32 len,         // [In]  input frame len ,  length of 2 channel bytes,
        qlUInt16 rete,        // [In]  Just support 16000
        qlUInt16 inchannel,   // [In]  Just support 2
        qlUInt16 outchannel,  // [In]  can be chose 1 or 2
        qlPInt16 pOutData,    // [Out] output frame data
        qlPUInt32 pOutDataLen // [Out] output frame len,  length of 1 or 2 channel bytes,
    );

    /**
     * @brief      : 销毁aec_ans句柄实例
     *
     * @param      : pobj           aec_ans句柄实例
     * @return     : qlStatus       返回处理状态
     */
    qlStatus qlCall aec_ans_destroy(
        qlPointer pObj // point to the aec_ans object
    );

    /**
     * @brief      : 获取aec_ans引擎的版本信息，主、次、修订、分支
     *
     * @param      : piMajor        主版本号
     * @param      : piMinor        次版本号
     * @param      : piRevision     修订版本号
     * @param      : piBranch       分支版本号
     * @return     : qlStatus       返回处理状态
     */
    qlStatus qlCall aec_ans_get_version(
        qlPUInt16 piMajor,    // [Out] main version
        qlPUInt16 piMinor,    // [Out] sub version
        qlPUInt16 piRevision, // [Out] build version
        qlPUInt16 piBranch    // [Out] branch version
    );

    /**
     * @brief      : 设置AEC_ANS降噪强度
     *
     * @param      : pobj           aec_ans句柄实例
     * @param      : nParamValue    参数值，范围是[0,1]，默认值是0.5
     * @return     : qlStatus       返回处理状态
     */
    qlStatus qlCall aec_ans_set_denoise_strength(
        qlPointer pObj,   // point to the aec_ans object
        float nParamValue // [In]  param value, in range of [0, 1], default is 0.5
    );

        /**
     * @brief      : 设置AEC_ANS降噪强度
     *
     * @param      : pobj           aec_ans句柄实例
     * @return     : qlStatus       返回处理状态
     */
    qlStatus qlCall aec_ans_reset(
        qlPointer pObj   // point to the aec_ans object
    );

#ifdef __cplusplus
}
#endif

#endif // QUECTEL_SPEECH_FRONTEND_ANS_AEC_ANS_API_H
