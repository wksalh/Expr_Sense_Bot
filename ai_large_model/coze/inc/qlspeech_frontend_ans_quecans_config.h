/**
 * *****************************************************************************
 * @file       : qlspeech_frontend_ans_quecans_config.h
 * @brief      : 语音前端QuecANS降噪算法配置头文件
 * @author     : rambo.zhang@quectel.com
 * @date       : 2024-12-02
 * @version    : v1.0
 * @copyright  : Copyright (c) 2024 Smart AI Robot
 * @license    : MIT License
 * @details    : 该头文件定义了QuecANS降噪算法的详细配置参数。
 *              包括采样率配置、降噪强度设置、帧长度配置、
 *              VAD开关、模型路径配置以及各种滤波器开关。
 *              提供了详细的平台适配和文件系统兼容性配置。
 * *****************************************************************************
 */

#ifndef QUECTEL_SPEECH_FRONTEND_ANS_QUECANS_CONFIG_H
#define QUECTEL_SPEECH_FRONTEND_ANS_QUECANS_CONFIG_H

#include "qlspeech_frontend_ans_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PI 3.14159265358979323846264338327

#ifdef QUECTEL_SPEECH_FRONTEND_ANS_CONFIG_H_20240718    // ---------------------------------------->// 选择使用本模块自己的配置还是上一层的配来置覆盖
#define ANS_QUECANS_SAMPLE_RATE                         (QUEC_ANS_SAMPLE_RATE)                     // default 8000, support 8000, 16000 and 48000
#define ANS_QUECANS_DENOISE_STRENGTH                    (QUEC_ANS_DENOISE_STRENGTH)                // default denoise strength
#define ANS_QUECANS_ENGINE_CHECK                        (QUEC_ANS_ENGINE_CHECK)                    // developer defined engine check value
#define QUECANS_ENABLE_VAD                              (QUEC_ANS_ENABLE_VAD)                      // default 1, voice avtivity detection

#define QUECANS_MODEL_FROM_FILE                                                                    // load model from source file

#define INTER_THREADS                                   (1)
#define INTRA_THREADS                                   (1)

#ifdef QUECANS_MODEL_FROM_FILE
#define QUECANS_MODEL_PATH                              (QUEC_ANS_MODEL_PATH_QUECANS)
#define QUECANS_MODEL_PATH_MAX_SIZE                     (512)
#endif
#else   // ---------------------------------------------------------------------------------------->// 选择使用本模块自己的配置还是上一层的配来置覆盖
#define ANS_QUECANS_SAMPLE_RATE                         (8000)                                     // default 8000, support 8000, 16000 and 48000
#define ANS_QUECANS_DENOISE_STRENGTH                    (float)(0.5)                               // default denoise strength
#define ANS_QUECANS_ENGINE_CHECK                        ((qlUInt32)0x12FA85E)                      // developer defined engine check value
#define QUECANS_ENABLE_VAD                              (0)                                        // default 1, voice avtivity detection
#ifndef QUECANS_TRAINING
#define QUECANS_MODEL_FROM_FILE                                                                    // load model from source file
#endif
#ifdef QUECANS_MODEL_FROM_FILE
#ifdef __linux__
#define QUECANS_MODEL_PATH                              ("./models/Exp002M138_8k_simple.onnx")
#else
#define QUECANS_MODEL_PATH                              ("U:/model.bin")
#endif
#endif
#endif  // ---------------------------------------------------------------------------------------->// 选择使用本模块自己的配置还是上一层的配来置覆盖

#if defined(__linux__)                                                                              // 兼容不同平台的文件系统
#define QFILE                                            FILE
#define ql_fopen                                         (fopen)
#define ql_fclose                                        (fclose)
#define ql_fread                                         (fread)
#define ql_fseek                                         (fseek)
#define ql_ftell                                         (ftell)
#else
#define QUECANS_QUECTEL_CAT1                                                                       // 使用特定平台的文件系统
#include "ql_fs.h"                                                                                  // 使用特定平台的文件系统
#endif

#define QUECANS_BAND_DIVISION_POLICY                    (0)                                        // 频谱子带划分方法：0-Valin's ERB; 1-大幅提升子带分辨率; 2-子带数量和频谱谱线数相等,也即最高分辨率

#define QUECANS_ENABLE_PITCH_FILTER                     (1)                                        // default 1, 基音滤波
#define QUECANS_ENABLE_HIGHPASS_FILTER                  (1)                                        // default 1, 高通滤波
#define QUECANS_USE_SUB_BIAS                            (0)                                        // 是否使用子偏置,使用后可以减小量化引入的误差,但会增加计算量

#if QUECANS_ENABLE_VAD
#define VAD_DELAY_NUM_FRAME                           (6)                                        // VAD向后看的帧数，会引入“帧数*10ms”的延迟
#endif

#define ANS_QUECANS_FRAME_LEN_SHIFT                     (2)

#if ANS_QUECANS_SAMPLE_RATE == 8000

#if ANS_QUECANS_FRAME_LEN_10MS_TINY
#define ANS_QUECANS_FRAME_LEN                           (80)
#define ANS_QUECANS_TIME_LEN                            (10)      // ANS_QUECANS_FRAME_LEN/QUEC_ANS_SAMPLE_RATE*1000
#define ANS_QUECANS_WINDOW_LEN                          (2 * ANS_QUECANS_FRAME_LEN)
#define ANS_QUECANS_FREQ_SIZE                           (ANS_QUECANS_FRAME_LEN + 1)
#elif ANS_QUECANS_FRAME_LEN_10MS
#define ANS_QUECANS_FRAME_LEN                           (80)
#define ANS_QUECANS_TIME_LEN                            (10)      // ANS_QUECANS_FRAME_LEN/QUEC_ANS_SAMPLE_RATE*1000
#define ANS_QUECANS_WINDOW_LEN                          (2 * ANS_QUECANS_FRAME_LEN)
#define ANS_QUECANS_FREQ_SIZE                           (ANS_QUECANS_FRAME_LEN + 1)
#elif ANS_QUECANS_FRAME_LEN_10MS_BIG
#define ANS_QUECANS_FRAME_LEN                           (80)
#define ANS_QUECANS_TIME_LEN                            (10)      // ANS_QUECANS_FRAME_LEN/QUEC_ANS_SAMPLE_RATE*1000
#define ANS_QUECANS_WINDOW_LEN                          (256)
#define ANS_QUECANS_FREQ_SIZE                           (129)
#define FILTER_LENGTH                                   (ANS_QUECANS_FRAME_LEN*40)
#elif ANS_QUECANS_FRAME_LEN_16MS
#define ANS_QUECANS_FRAME_LEN                           (128)
#define ANS_QUECANS_TIME_LEN                            (16)      // ANS_QUECANS_FRAME_LEN/QUEC_ANS_SAMPLE_RATE*1000
#define ANS_QUECANS_WINDOW_LEN                          (2 * ANS_QUECANS_FRAME_LEN)
#define ANS_QUECANS_FREQ_SIZE                           (ANS_QUECANS_FRAME_LEN + 1)
#elif ANS_QUECANS_FRAME_LEN_16MS_BIG
#define ANS_QUECANS_FRAME_LEN                           (128)
#define ANS_QUECANS_TIME_LEN                            (16)      // ANS_QUECANS_FRAME_LEN/QUEC_ANS_SAMPLE_RATE*1000
#define ANS_QUECANS_WINDOW_LEN                          (2 * ANS_QUECANS_FRAME_LEN)
#define ANS_QUECANS_FREQ_SIZE                           (ANS_QUECANS_FRAME_LEN + 1)
#else
#define ANS_QUECANS_FRAME_LEN                           (128)
#define ANS_QUECANS_TIME_LEN                            (16)      // ANS_QUECANS_FRAME_LEN/QUEC_ANS_SAMPLE_RATE*1000
#define ANS_QUECANS_WINDOW_LEN                          (2 * ANS_QUECANS_FRAME_LEN)
#define ANS_QUECANS_FREQ_SIZE                           (ANS_QUECANS_FRAME_LEN + 1)
#endif

#define AND_QUECANS_NCICLEBUFLEN                        (5)                                     // 缓存池的大小，指有多少个帧

#define ANS_QUECANS_NB_BANDS                            ()

#elif ANS_QUECANS_SAMPLE_RATE == 16000                                                             // TODO:
#if ANS_QUECANS_FRAME_LEN_10MS_BIG
#define ANS_QUECANS_FRAME_LEN                           (160)
#define ANS_QUECANS_TIME_LEN                            (10)                                      // ANS_QUECANS_FRAME_LEN/QUEC_ANS_SAMPLE_RATE*1000
#define ANS_QUECANS_WINDOW_LEN                          (512)
#define ANS_QUECANS_FREQ_SIZE                           (257)
#define FILTER_LENGTH                                   (ANS_QUECANS_FRAME_LEN*40)
#endif
#define AND_QUECANS_NCICLEBUFLEN                        (100)                                     // 缓存池的大小，指有多少个帧
#define ANS_QUECANS_NB_BANDS                            ()
#elif ANS_QUECANS_SAMPLE_RATE == 48000                                                             // TODO:
#define ANS_QUECANS_FRAME_LEN                           ()
#define ANS_QUECANS_NB_BANDS                            ()
#endif

#ifdef __cplusplus
}
#endif

#endif // QUECTEL_SPEECH_FRONTEND_ANS_QUECANS_CONFIG_H
