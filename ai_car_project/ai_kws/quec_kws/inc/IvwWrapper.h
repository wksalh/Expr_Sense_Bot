#pragma once
/***********************************************************************
desc:	引擎库封装类，封装了引擎库的基础调用逻辑
author:	ljliu5
data:	2020/06/04
***********************************************************************/
/*
 *  引入引擎头文件
 */
#include "ivw/ivw_defines.h"
#include "ivw/ivw_type.h"
#include "ivw/wivw_api.h"
#include <pulse/simple.h>
#include <pulse/error.h>
/*
 *  Demo代码中依赖的头文件，主要用于参数解析、字符串拆分、异常检查
 */
#include "defines.h"
#include <string>
#include <vector>
#include <semaphore>
#include "ai_audio_init.h"
#include <semaphore.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "ai_common_ringbuffer.h"
#include "ai_kws_ops.h"

class Semaphore {
private:
    std::mutex mtx;
    std::condition_variable cv;
    int count;
public:
    Semaphore(int initial = 0) : count(initial) {}

    void acquire() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return count > 0; });
        count--;
    }

    void release() {
        std::lock_guard<std::mutex> lock(mtx);
        count++;
        cv.notify_one();
    }
};

class IvwWrapper
{
public:
	IvwWrapper();
	~IvwWrapper();

public:
	/*  
	 *	加载引擎库
	 *  参数:
	 *		pDllPath  引擎库文件路径，不可为空
	 */
	ivInt loadLibrary(_In_ const ivChar* pDllPath);

	/*
	 *	释放引擎库
	 */
	ivInt freeLibrary();

	/*
	 *	初始化引擎管理类，设置进程级参数
	 */
	ivInt initIvwManager();

	/*
	 *	销毁引擎管理类
	 */
	ivInt uninitIvwManager();

	/* 
	 * 加载唤醒功能需要的所有资源
	 *
	 * 参数:
	 *    pMlpResPath		IVW_MLP资源文件路径
	 *    pFillerResPath	IVW_FILLER资源文件路径
	 *    pKeywordResPath	IVW_KEYWORD资源文件路径
	 */
	ivInt loadAllRes(_In_ const ivChar *pMlpResPath, _In_ const ivChar *pFillerResPath, _In_ const ivChar *pKeywordResPath);

	/*
	 *	释放所有资源
	 */
	ivInt destoryAllRes();

	/* 
	 *	创建唤醒实例，设置唤醒实例的回调函数
	 */
	ivInt createIvwInstAndSetCallback();

	/* 
	 *	销毁唤醒实例
	 */
	ivInt destroyIvwInst();

	/*
	 *	设置实例级参数，并开始实例
	 *	参数:
	 *		pAudioFilePath	当前识别音频的标识
	 */
	ivInt startIvwInst(_In_opt_ const ivChar *pAudioFilePath);

	/*
	 *	停止唤醒实例
	 */
	ivInt stopIvInst();

	/* 处理唤醒回调
	 * 参数:
	 *   pIvwResult 唤醒结果字符串
	 */
	ivInt procIvwCallback(_In_ const ivChar *pIvwResult);

	/* 写入音频
	 * 参数:
	 *   audioBuffer	音频buf
	 *   bufLen			s音频buf长度 
	 *   nWriteState	写入状态
	 */
	ivInt writeAudio(_In_ const ivChar *pAudioBuffer, _In_ ivInt bufLen, _In_ ivInt nWriteState);

	void kws_set_ops(module_desc_t* self_ops)
	{
		kws_ops = self_ops;
	}

    ivInt audio_thread_start();

	void kws_thread_function();

	// ivInt set_on_callback(CallbackFuncs callback);

	ivInt post_sem();

	void audio_kws_callback(void* data, int data_len);
private:
	ivInt loadResFileIntoMemory(_In_ const ivChar *pResFilePath, _Out_ ivChar **pResBuf, _Out_ ivUInt *pResSize);

private:
	/*
	 *	唤醒管理类相关的变量
	 */
	IvwInterface *pMgr_;

	/*
	 *	唤醒实例相关的变量
	 */
	PIvwInstBase pIvwInst_;
	
	static IVW_RES_SET ivwInstResSet[3];

	/*
	 *	引擎库相关的变量
	 */
	DLL_HANDLE hdll_;
	Proc_wIvwInitialize			wIvwInitialize_;
	Proc_wIvwUninitialize		wIvwUninitialize_;

	/*
	 *	唤醒结果输出文件
	 */
	FILE *pFile_;

	// CallbackFuncs my_callback;
	Semaphore audio_run_sem{0};
	bool kws_rung_flag=true;
	bool bIvwInstStart_;
	bool kws_exit_flag=true;
	RingBuffer* kws_audio_buffer;
	std::thread kws_audio_thread;
	std::thread playaudio_thread;
	module_desc_t* kws_ops = nullptr;
};


