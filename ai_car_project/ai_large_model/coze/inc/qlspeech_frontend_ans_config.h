/**
 * *****************************************************************************
 * @file       : qlspeech_frontend_ans_config.h
 * @brief      : 语音前端ANS降噪算法顶层配置头文件
 * @author     : lay.liu@quectel.com
 * @date       : 2024-07-19
 * @version    : v1.0
 * @copyright  : Copyright (c) 2024 Smart AI Robot
 * @license    : MIT License
 * @details    : 该头文件定义了语音前端ANS降噪算法的顶层配置。
 *              作为SDK编译时的条件编译选项，可配置引擎的默认工作状态。
 *              包括OMLSA、WIENER、RNNOISE2、VAD、AGC等模块的开关，
 *              以及试用版设置、模型加载方式和默认参数配置。
 * *****************************************************************************
 */

#ifndef QUECTEL_SPEECH_FRONTEND_ANS_CONFIG_H_20240718
#define QUECTEL_SPEECH_FRONTEND_ANS_CONFIG_H_20240718

#define QUEC_ANS_ENABLE_OMLSA               (0)                                         // (条件编译) 启用OMLSA降噪,默认0

#define QUEC_ANS_ENABLE_WIENER              (0)                                         // (条件编译) 启用WIENER降噪,默认1

#define QUEC_ANS_ENABLE_RNNOISE2            (0)                                         // (条件编译) 启用RNNOISE2降噪,默认1

#define QUEC_ANS_ENABLE_VAD                 (0)                                         // (已提供接口) (条件编译) 启用人声检测,会额外引入60ms延迟,默认1

#define QUEC_ANS_ENABLE_AGC                 (0)                                         // (条件编译) 启用自动增益控制,默认1

#define QUEC_ANS_TRIAL                      (0)                                         // (条件编译) 试用版试制,限时2小时

#define QUEC_ANS_MODEL_FILE                 (0)                                         // (条件编译) 模型以二进制文件方式加载,默认1,如果设置为0,模型会以静态数组的方式打包进库,根据堆栈的空间需求自行设置

// #define ENABLE_ONNXRUNTIME                                                              // choose onnxruntime DeepLearnFramework for AI

#define ENABLE_MNN                                                                      // choose MNN DeepLearnFramework for AI

#define WATERMARK_ENABLE                        (1)

#define ANS_QUECANS_FRAME_LEN_10MS_TINY           (0)
#define ANS_QUECANS_FRAME_LEN_10MS                (0)
#define ANS_QUECANS_FRAME_LEN_10MS_BIG            (1)
#define ANS_QUECANS_FRAME_LEN_16MS                (0)
#define ANS_QUECANS_FRAME_LEN_16MS_BIG            (0)

#define QUEC_ANS_MODEL_PATH_RNNOISE2        ("./models/model_EXP003M200.bin")           // (已提供接口) 以二进制文件加载模型时,模型的默认路径，给RNNOISE2使用

#ifdef ENABLE_ONNXRUNTIME
#if ANS_QUECANS_FRAME_LEN_10MS_TINY
#define QUEC_ANS_MODEL_PATH_QUECANS           ("./models/Exp010M043_08k_mdl-v1.3_10ms_20ms_lite_simple.onnx")      // (已提供接口) 以二进制文件加载模型时,模型的默认路径，给QUECANS使用
#elif ANS_QUECANS_FRAME_LEN_10MS
#define QUEC_ANS_MODEL_PATH_QUECANS           ("./models/Exp005M066_08k_model-v1.2_10ms_20ms_simple.onnx")      // (已提供接口) 以二进制文件加载模型时,模型的默认路径，给QUECANS使用
#elif ANS_QUECANS_FRAME_LEN_10MS_BIG
#define QUEC_ANS_MODEL_PATH_QUECANS           ("./models/Exp006M032_08k_mdl-v1.0_10ms_32ms.onnx")      // (已提供接口) 以二进制文件加载模型时,模型的默认路径，给QUECANS使用
#elif ANS_QUECANS_FRAME_LEN_16MS
#define QUEC_ANS_MODEL_PATH_QUECANS           ("./models/Exp002M138_8k_simple.onnx")      // (已提供接口) 以二进制文件加载模型时,模型的默认路径，给QUECANS使用
#elif ANS_QUECANS_FRAME_LEN_16MS_BIG
#define QUEC_ANS_MODEL_PATH_QUECANS           ("./models/Exp003M048_08k_model-v1.1_16ms_32ms_simple.onnx")      // (已提供接口) 以二进制文件加载模型时,模型的默认路径，给QUECANS使用
#endif
#endif

#ifdef ENABLE_MNN
#if ANS_QUECANS_FRAME_LEN_10MS_TINY
#define QUEC_ANS_MODEL_PATH_QUECANS           ("./models/Exp010M043_08k_mdl-v1.3_10ms_20ms_lite_simple.mnn")      // (已提供接口) 以二进制文件加载模型时,模型的默认路径，给QUECANS使用
#elif ANS_QUECANS_FRAME_LEN_10MS
#define QUEC_ANS_MODEL_PATH_QUECANS           ("./models/Exp005M066_08k_model-v1.2_10ms_20ms_simple.mnn")     // (已提供接口) 以二进制文件加载模型时,模型的默认路径，给QUECANS使用
#elif ANS_QUECANS_FRAME_LEN_10MS_BIG
#define QUEC_ANS_MODEL_PATH_QUECANS           ("./models/Exp006M032_08k_mdl-v1.0_10ms_32ms.onnx")              // (已提供接口) 以二进制文件加载模型时,模型的默认路径，给QUECANS使用
#elif ANS_QUECANS_FRAME_LEN_16MS
#define QUEC_ANS_MODEL_PATH_QUECANS           ("/storage/emulated/0/models/Exp002M138_8k_simple.mnn")         // (已提供接口) 以二进制文件加载模型时,模型的默认路径，给QUECANS使用
#elif ANS_QUECANS_FRAME_LEN_16MS_BIG
#define QUEC_ANS_MODEL_PATH_QUECANS           ("./models/Exp003M048_08k_model-v1.1_16ms_32ms_simple.mnn")     // (已提供接口) 以二进制文件加载模型时,模型的默认路径，给QUECANS使用
#endif
#endif

#define QUEC_ANS_DENOISE_STRENGTH           (0.5)                                       // (已提供接口) 设置降噪强度,取值范围[0,1],值越大降噪强度越强,默认0.5
        
#define QUEC_ANS_AGC_TARGET_DBFS            (-4.5)                                      // (已提供接口) 设置AGC目标分贝数(全量程),默认-3
        
#define QUEC_ANS_AGC_ENABLE_LIMITER         (1)                                         // (已提供接口) 设置是否启用AGC中的限幅器,默认不启用
        
#define QUEC_ANS_SAMPLE_RATE                (16000)                                      // 采样频率,现在支持8000Hz
        
#define QUEC_ANS_ENGINE_CHECK               ((qlUInt32)0x12FA85E)                       // 引擎检查关键字

#endif // QUECTEL_SPEECH_FRONTEND_ANS_CONFIG_H_20240718
