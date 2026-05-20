
#include <semaphore.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <locale.h>
#include <pthread.h>
#include <time.h>
#include <assert.h>
#include "ql_WakeUpAPI.h"
#include "ivESR.h"
#include "fftw3.h"
#include "ai_common_shm.h"
#include "gbk_unicode.h"
#include "ai_audio_play.h"
#include "ai_local_asr.h"
#include "ai_websocket.h"
#include "ai_audio.h"
#include "ai_audio_init.h"
#include "ai_common_define.h"
#include <cjson/cJSON.h>
#include "ai_log.h"

static sem_t asr_sem;
static RingBuffer *asr_Aduio_buffer;
static pthread_t   asr_pthread_id;
static ql_WakeUpInterface ql_WakeUpHandle;
static module_desc_t* locar_asr_desc = NULL;

typedef struct {
    bool is_speech;
    int speech_frames;
    int silence_frames;
} VADState;

VADState vad_state = {false, 0, 0};
static bool asr_run_flag=false;
static bool asr_thread_running=false;
static bool asr_sem_initialized=false;
static pthread_mutex_t asr_state_mutex = PTHREAD_MUTEX_INITIALIZER;

static void set_asr_run_flag(bool flag)
{
	pthread_mutex_lock(&asr_state_mutex);
	asr_run_flag = flag;
	pthread_mutex_unlock(&asr_state_mutex);
}

static bool get_asr_run_flag(void)
{
	bool flag;
	pthread_mutex_lock(&asr_state_mutex);
	flag = asr_run_flag;
	pthread_mutex_unlock(&asr_state_mutex);
	return flag;
}

static void set_asr_thread_running(bool flag)
{
	pthread_mutex_lock(&asr_state_mutex);
	asr_thread_running = flag;
	pthread_mutex_unlock(&asr_state_mutex);
}

static bool get_asr_thread_running(void)
{
	bool flag;
	pthread_mutex_lock(&asr_state_mutex);
	flag = asr_thread_running;
	pthread_mutex_unlock(&asr_state_mutex);
	return flag;
}

void ResultLog(PEsrResult pResult, ivUInt32 nResult)
{
	(void)nResult;
	ivUInt32 i;
	int Action_ID=2000,Action_Time=10000;
	char* messages ="./../config/5.wav";
	for (i = 0; i < pResult->nSlot; i++)
	{
		char pLine[MAX_PATH];
		memset(pLine, 0, MAX_PATH);
		ucs2utf8s(pResult->pSlots[i].pItems->pText, pLine);
		if (!strcmp(pLine, "<s>") || !strcmp(pLine, "</s>"))
			continue;
		quec_ai_log(LOG_AI_INFO,"\t\t< Rec = %s >\n", pLine);
		quec_ai_log(LOG_AI_INFO,"\t\t< ID = %d >\n", pResult->pSlots[i].pItems->nID);
		if(pResult->pSlots[i].pItems->nID>CAR_ACTION_START && pResult->pSlots[i].pItems->nID<CAR_ACTION_END)
		{
			Action_ID=pResult->pSlots[i].pItems->nID-2000;
		}
		if(pResult->pSlots[i].pItems->nID>3000)
		{
			Action_Time=pResult->pSlots[i].pItems->nID-3000;
		}
	}
	quec_ai_log(LOG_AI_INFO,"\t\t< score = %d,%d,%d >\n", pResult->nConfidenceScore,Action_ID,CAR_EXIT-2000);
	if(Action_ID==(CAR_EXIT-2000) && pResult->nConfidenceScore>-50)
	{
		set_asr_run_flag(false);
		common_msg_t *msg_2= ai_common_create_msg(AUDIO_FILE_PLAY_EVENT,messages,0,1);
		locar_asr_desc->private_data->ops->set_param(msg_2);
		common_msg_t *msg = ai_common_create_msg(KWS_START_EVENT,NULL,0,1);
		locar_asr_desc->private_data->ops->set_param(msg);
		return ;
	}
	if(Action_ID!=(CAR_EXIT-2000)&&pResult->nConfidenceScore>500)
	{
		quec_ai_set_asr_connect_time(time(NULL));
		char* message="./../config/4.wav";
		common_msg_t *msg_2= ai_common_create_msg(AUDIO_FILE_PLAY_EVENT,message,0,1);
		locar_asr_desc->private_data->ops->set_param(msg_2);
		send_ttlv_data_to_ros2(Action_ID,Action_Time,true);
	}

}

/* 消息回调函数 */
ivStatus ivCall CBMsgProc(ivPointer pUserData, ivHandle hObj, ivUInt32 uMsg, ivUInt32 wParam, ivCPointer lParam)
{
	// PUserData pUser = (PUserData)pUserData;/*EsrGetUserData(hObj)*/
	(void)pUserData;
	(void)hObj;
	switch (uMsg)
	{
	case ivMsg_ToSleep:
		usleep(wParam * 1000);
		break;
	case ivMsg_Create:
	case ivMsg_GRMCreate:
		// InitializeCriticalSection(&pUser->tCriticalSection);
		break;
	case ivMsg_Destroy:
	case ivMsg_GRMDestroy:
		// DeleteCriticalSection(&pUser->tCriticalSection);
		break;
	case ivMsg_ToStartAudioRec:
		quec_ai_log(LOG_AI_INFO,"\t开始录音...\r\n");
		break;
	case ivMsg_ToStopAudioRec:
		quec_ai_log(LOG_AI_INFO,"\t结束录音...\r\n");
		break;
	case ivMsg_ToEnterCriticalSection:
		// EnterCriticalSection(&pUser->tCriticalSection);
		break;
	case ivMsg_ToLeaveCriticalSection:
		// LeaveCriticalSection(&pUser->tCriticalSection);
		break;
	case ivMsg_SpeechStart:
		quec_ai_log(LOG_AI_INFO,"\t检测到语音...\r\n");
		break;
	case ivMsg_SpeechEnd:
		quec_ai_log(LOG_AI_INFO,"\t语音结束...\r\n");
		break;
	case ivMsg_ResponseTimeout:
		quec_ai_log(LOG_AI_INFO,"\t反应超时...\r\n");
		usleep(200);

		// // plan A
		// PEsrResult pResult = (PEsrResult) malloc(sizeof(TEsrResult));
		// memset(pResult, 0, sizeof(TEsrResult));
		// pResult->nSlot = 1;
		// pResult->pSlots = (PSlotInfo) malloc(sizeof(TSlotInfo));
		// memset(pResult->pSlots, 0, sizeof(TSlotInfo));
		// pResult->nConfidenceScore = -1000;
		// pResult->pSlots->pItems = (PWordItem) malloc(sizeof(PWordItem));
		// memset(pResult->pSlots->pItems, 0, sizeof(PWordItem)); 
		// pResult->pSlots->pItems->pText = (unsigned short *)malloc(sizeof(char) * 20);
		// memset((void *)pResult->pSlots->pItems->pText, 0, sizeof(char) * 20);
		// memcpy((void *)pResult->pSlots->pItems->pText, L"error", strlen("error"));
		// ResultLog(pResult, 0);
		// free((void *)pResult->pSlots->pItems->pText);
		// free(pResult->pSlots->pItems);
		// free(pResult->pSlots);
		// free(pResult);

		// // plan B
		// PEsrResult pResult = (PEsrResult) malloc(sizeof(TEsrResult));
		// memset(pResult, 0, sizeof(TEsrResult));
		// pResult->nSlot = 1;
		// pResult->nConfidenceScore = -32769;
		// // 这里面首先对nConfidenceScore进行判决，低于-32768的是无效的结果，无需再判别文本内容
		// ResultLog(pResult, 0);
		// free(pResult);

		break;
	case ivMsg_SpeechTimeout:
		quec_ai_log(LOG_AI_INFO,"\t语音超时超时...\r\n");
		usleep(200);
		break;
	case ivMsg_Result:
		quec_ai_log(LOG_AI_INFO,"\t识别结果...\r\n");
		if (lParam == NULL)
		{
			quec_ai_log(LOG_AI_INFO,"\tEsrResult = NULL\n");
			break;
		}
		ResultLog((PEsrResult)lParam, (ivUInt32)wParam);
		break;
	case ivMsg_RecognizeEnd:
		break;
	case ivMsg_LOG:
		break;
	case ivMsg_Error:
		quec_ai_log(LOG_AI_INFO,"\t识别服务返回错误码消息: %d\r\n", wParam);
		break;
	case ivMsg_DestroyWaiting:
		quec_ai_log(LOG_AI_INFO,"\t识别对象暂时无法销毁：正在被使用...请等待...\r\n");
		break;
	case ivMsg_GRMDestroyWaiting:
		quec_ai_log(LOG_AI_INFO,"\t语法对象暂时无法销毁：正在被使用...请等待...\r\n");
		break;
	case ivMsg_EsrEngineState:
		if (wParam == ivErr_Started)
		{
			quec_ai_log(LOG_AI_INFO,"\t识别引擎资源初始化成功...\r\n");
		}
		else if (wParam == ivErr_Stopped)
		{
			quec_ai_log(LOG_AI_INFO,"\t识别引擎成功停止...\r\n");
		}
		else if (wParam == ivErr_AlreadyStarted)
		{
			quec_ai_log(LOG_AI_INFO,"\t识别引擎已经启动,启动命令被忽略...\r\n");
		}
		else if (wParam == ivErr_AlreadyStopped)
		{
			quec_ai_log(LOG_AI_INFO,"\t识别引擎已经停止,停止命令被忽略...\r\n");
		}
		else
		{
			quec_ai_log(LOG_AI_INFO,"\t识别引擎已经异常，请检查....错误码消息：%d\r\n", wParam);
		}
		break;
	case ivMsg_CommitNetWork:
		quec_ai_log(LOG_AI_INFO,"\tCommit Grm %p Over Return = %d\n", lParam, wParam);
		break;
	}
	return ivErr_OK;
}

/* 打开文件回调函数 */
ivHandle ivCall CBOpenFile(ivPointer pUser, ivCStr lpFileName, ivInt enMod, ivInt enType)
{
	(void)pUser;
  FILE *pf;
  char szFileName[MAX_PATH];
  char *lpszMod;
  const char *base_path;
//   quec_ai_log(LOG_AI_INFO,"CBOpenFile: %s OK:\t%d\n",__func__, __LINE__);
  if (ivResFile == enType)
  {
    base_path = "./../config/Resource/";
  }
  else
  {
    base_path = "./../config/GrmResource/";
  }
  if (snprintf(szFileName, sizeof(szFileName), "%s%s", base_path, lpFileName ? (const char *)lpFileName : "") >= (int)sizeof(szFileName))
  {
    quec_ai_log(LOG_AI_INFO,("%s\t%d:\t file path too long\r\n"), __func__, __LINE__);
    return NULL;
  }
  if (ivModWrite == enMod)
  {
    lpszMod = ("wb");
  }
  else
  {
    lpszMod = ("rb");
  }
  // quec_ai_log(LOG_AI_INFO,"%s OK:\t%d\n",__func__, __LINE__);
  pf = fopen(szFileName, lpszMod);
  // quec_ai_log(LOG_AI_INFO,"%s OK:\t%d\n",__func__, __LINE__);
  if (pf == NULL)
    quec_ai_log(LOG_AI_INFO,("%s\t%d:\t %s err\r\n"), __func__, __LINE__, szFileName);
  // quec_ai_log(LOG_AI_INFO,"%s OK:\t%d\n",__func__, __LINE__);
  return (ivHandle)pf;
}

static void asr_audio_callbnack(void* data, int data_len)
{
    ringbuffer_write(asr_Aduio_buffer,(char*)data,data_len);
}



void run_asr_deal()
{
    int sem_value;
    if (!asr_sem_initialized) {
        quec_ai_log(LOG_AI_INFO,"asr sem not initialized\n");
        return;
    }
	sem_getvalue(&asr_sem, &sem_value);
	if(sem_value>0)
	{
		return ;
	}else{
		 if (sem_post(&asr_sem) == -1) {
            quec_ai_log(LOG_AI_INFO,"post asr_sem sem error\n");
        }
		set_asr_run_flag(true);
	}
}

void stop_asr_deal()
{
    set_asr_run_flag(false);
    set_asr_thread_running(false);
    if (asr_sem_initialized) {
        sem_post(&asr_sem);
    }
}

static bool  fftw_deal(char* buffer,double *audio_double,fftw_complex *out,fftw_plan plan)
{
	for (unsigned int  i = 0; i < SAMPLES_PER_FRAME; i++) {
		audio_double[i] = (double)(((short *)buffer)[i]) / 32768.0;
	}
	
	// 2. 执行 FFT
	fftw_execute(plan);

	// 3. 计算低频能量（300Hz–3000Hz）
	int low_freq_start = 300 * (SAMPLES_PER_FRAME / 2 + 1) / ASR_SAMPLE_RATE;
	int low_freq_end   = 3000 * (SAMPLES_PER_FRAME / 2 + 1) / ASR_SAMPLE_RATE;
	float low_freq_energy = 0;
	for (int i = low_freq_start; i < low_freq_end; i++) {
		low_freq_energy += out[i][0] * out[i][0] + out[i][1] * out[i][1];
	}
	// 4. 动态阈值判断
	static float bg_energy = 0;
	static int frame_count = 0;
	static float threshold = 2000; // 初始阈值
	

	if (low_freq_energy < bg_energy * 1.5) {
		bg_energy = 0.9 * bg_energy + 0.1 * low_freq_energy;
		frame_count++;
		if (frame_count > 10) {
			threshold = bg_energy * 2.0;
		}
	}
	 // 5. 判断是否为语音
	bool current_speech = low_freq_energy > threshold;
	// quec_ai_log(LOG_AI_INFO,"(能量: %.2f, %d, %d, %d)\n", low_freq_energy, current_speech, vad_state.silence_frames, vad_state.speech_frames);
	
	return current_speech;
}

void* asr_audio_thread(void* arg) {
    (void)arg;
    char buffer[OFFLINE_AUDIO_SIZE];
    double *audio_double = (double *)fftw_malloc(SAMPLES_PER_FRAME * sizeof(double));
    fftw_complex *out = fftw_malloc(sizeof(fftw_complex) * (SAMPLES_PER_FRAME / 2 + 1));
    static bool in_speech = false;
    char* lookback_buffer=(char*)malloc(ASR_RING_BUFFER_SIZE);
    int lookback_write_pos = 0;
    int lookback_frame_count = 0;
	fftw_plan plan = NULL;

    if (audio_double == NULL || out == NULL || lookback_buffer == NULL) {
        quec_ai_log(LOG_AI_INFO,"local asr malloc error\n");
        goto cleanup;
    }
	plan = fftw_plan_dft_r2c_1d(SAMPLES_PER_FRAME, audio_double, out, FFTW_ESTIMATE);
    if (plan == NULL) {
        quec_ai_log(LOG_AI_INFO,"local asr fftw plan error\n");
        goto cleanup;
    }
	// struct timespec start, end;
	// double elapsed_time;
    while (get_asr_thread_running()) {
		quec_ai_log(LOG_AI_INFO,"local asr wait sem\n");
		quec_ai_set_online_asr_flag(false);
        sem_wait(&asr_sem);
        if (!get_asr_thread_running()) {
            break;
        }
		quec_ai_set_online_asr_flag(true);
        while (get_asr_run_flag()) {
            int len = ringbuffer_read_wait(asr_Aduio_buffer, buffer, OFFLINE_AUDIO_SIZE);
            if (len != OFFLINE_AUDIO_SIZE) {
                quec_ai_log(LOG_AI_INFO,"ASR 读取音频数据错误, %d\n", len);
                continue;
            }
			// clock_gettime(CLOCK_MONOTONIC, &start);
            bool current_speech = fftw_deal(buffer, audio_double, out,plan);
            // 更新 lookback buffer
            memcpy(lookback_buffer+(lookback_write_pos * OFFLINE_AUDIO_SIZE), buffer, OFFLINE_AUDIO_SIZE);
            // lookback_write_pos = (lookback_write_pos + OFFLINE_AUDIO_SIZE) % ASR_RING_BUFFER_SIZE;
			lookback_write_pos = (lookback_write_pos + 1) % LOOKBACK_FRAMES ;
            if (lookback_frame_count < LOOKBACK_FRAMES) {
                lookback_frame_count++;
            }

            if (current_speech) {
                vad_state.silence_frames = 0;
                if (!in_speech) {
                    quec_ai_log(LOG_AI_INFO,"检测到语音开始\n");
                    in_speech = true;
                    ql_WakeUp_Start(&ql_WakeUpHandle);
                    vad_state.is_speech = true;
					// 回溯时，读取 lookback_frame_count - 1 帧（跳过最新的一帧）
					if (lookback_frame_count > 0) {
						int frames_to_read = lookback_frame_count - 1; // 跳过最新的一帧
						if (frames_to_read <= 0) {
							// 没有足够的历史数据
							continue;
						}

						if (frames_to_read <= LOOKBACK_FRAMES) {
							// 计算起始位置：从最旧的有效帧开始
							// lookback_write_pos 指向下一个写入位置，所以要减去总帧数
							int start_pos = (lookback_write_pos - lookback_frame_count + LOOKBACK_FRAMES) % LOOKBACK_FRAMES;
							quec_ai_log(LOG_AI_INFO,"发送历史音频: 从位置 %d 开始, 共 %d 帧\n", start_pos, frames_to_read);
							// 按时间顺序读取 frames_to_read 帧
							for (int i = 0; i < frames_to_read; i++) {
								int read_pos = (start_pos + i) % LOOKBACK_FRAMES;
								
								// 再次检查：确保不是最新帧
								int latest_frame_pos = (lookback_write_pos - 1 + LOOKBACK_FRAMES) % LOOKBACK_FRAMES;
								if (read_pos == latest_frame_pos) {
									quec_ai_log(LOG_AI_INFO,"警告: 跳过了最新帧 %d\n", read_pos);
									continue; // 跳过最新帧
								}
								// 发送历史音频数据
								ql_WakeUp_AppendAudioData(&ql_WakeUpHandle, 
														(short*)(lookback_buffer + (read_pos * OFFLINE_AUDIO_SIZE)), 
														SAMPLES_PER_FRAME);
								// fwrite(lookback_buffer + (read_pos * OFFLINE_AUDIO_SIZE), 1, OFFLINE_AUDIO_SIZE, fp1);
							}
						}
					}
                }
                vad_state.speech_frames++;
            } else {
                vad_state.silence_frames++;
                vad_state.speech_frames = 0;
                if (in_speech && vad_state.silence_frames > 6) {
                    quec_ai_log(LOG_AI_INFO,"检测到语音结束\n");
                    in_speech = false;
                    ql_WakeUp_Stop(&ql_WakeUpHandle);
                    vad_state.is_speech = false;
                }
            }
            if (in_speech) {
                ql_WakeUp_AppendAudioData(&ql_WakeUpHandle, (short*)buffer, SAMPLES_PER_FRAME);
            }
			  // 获取结束时间
    		// clock_gettime(CLOCK_MONOTONIC, &end);
    			// 计算执行时间（纳秒）
    		// elapsed_time = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
			// quec_ai_log(LOG_AI_INFO,"函数执行时间: %.3f 毫秒\n", elapsed_time / 1e6);
        }
    }
cleanup:
	if (plan) {
		fftw_destroy_plan(plan);
	}
    if (audio_double) {
        fftw_free(audio_double);
    }
    if (out) {
        fftw_free(out);
    }
    free(lookback_buffer);
    return NULL;
}


int Intelligent_Asr_Init(module_desc_t * self_ops)
{
    if (sem_init(&asr_sem, 0, 0) != 0) {
        quec_ai_log(LOG_AI_INFO,"asr sem init error\n");
        return -1;
    }
    asr_sem_initialized = true;
    set_asr_thread_running(true);
    ivStatus iStatus;
	char bnfResource[MAX_PATH] = "./../config/default.bnf";
    locar_asr_desc=self_ops;
	ql_WakeUpHandle.pCallBackMSG = CBMsgProc;
	ql_WakeUpHandle.pCallBackOpenFile = CBOpenFile;
    quec_ai_log(LOG_AI_INFO,"strlen(bnfResource) = %ld\n", strlen(bnfResource));
	memset(ql_WakeUpHandle.bnfFile, 0, MAX_PATH);
	memcpy(ql_WakeUpHandle.bnfFile, bnfResource, strlen(bnfResource));

    quec_ai_log(LOG_AI_INFO,"%s\t%d:\tstep1: Create WakeUp Handle\n",__func__,__LINE__);
    iStatus = ql_WakeUp_Create(&ql_WakeUpHandle);
	usleep(200000);
	if (iStatus != ivErr_OK)
	{
		quec_ai_log(LOG_AI_INFO,"%s\t%d:\tErr ret = %d\n", __func__, __LINE__, iStatus);
        sem_destroy(&asr_sem);
        asr_sem_initialized = false;
        set_asr_thread_running(false);
		return -1;
	}
    asr_Aduio_buffer = ringbuffer_init(ASR_RING_BUFFER_SIZE);
    if(asr_Aduio_buffer==NULL)
    {
        quec_ai_log(LOG_AI_INFO,"ringbuffer init error\n");
        sem_destroy(&asr_sem);
        asr_sem_initialized = false;
        set_asr_thread_running(false);
        return -1;
    }
	quec_ai_audio_register_data_callback(asr_audio_callbnack);
    if (pthread_create(&asr_pthread_id, NULL, asr_audio_thread, NULL) != 0) {
        ringbuffer_destroy(asr_Aduio_buffer);
        asr_Aduio_buffer = NULL;
        sem_destroy(&asr_sem);
        asr_sem_initialized = false;
        set_asr_thread_running(false);
        return -1;
    }
	struct sched_param param;
    param.sched_priority = 0;
	pthread_setschedparam(asr_pthread_id, SCHED_OTHER, &param);
    pthread_detach(asr_pthread_id);
    return 0;
}
