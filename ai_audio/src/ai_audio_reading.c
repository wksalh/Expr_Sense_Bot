/**
 * @file        ai_audio_reading.c
 * @brief       音频读取模块实现
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该文件实现了音频读取功能，包括麦克风阵列读取、AEC回声消除等。
 */

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "ai_audio.h"
#include "ai_common_list.h"
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>
#include "ai_websocket_echo_cancellation.h"
#include "qlspeech_frontend_aec_ans_api.h"
#include <string.h>
#include "ai_local_asr.h"
#include "ai_audio_play.h"
#include <cjson/cJSON.h>
#include "ai_log.h"
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include "ai_audio_init.h"
#include "ai_array_mic.h"

static pa_simple*  pulse_audio_read_handle=NULL; 
static pa_simple *monitor_pa = NULL;  // 监听流句柄
static qlPointer echo_pObj=NULL;
Websocket_Config* web_config;
static pthread_t  read_pthread_id;

CallbackList read_callback_list;

void quec_ai_audio_register_data_callback(DataCallback callback)
{
    callback_list_register(&read_callback_list,callback);
}

void* read_audio_pthread(void* arg)
{
    CallbackList* list = (CallbackList*)arg;
    char audio_data[KWS_AUDIO_FRAME_LEN];
    char speaker_data[KWS_AUDIO_FRAME_LEN];  // 喇叭输出数据
    char  output_data[KWS_AUDIO_FRAME_LEN];
    int error;
    qlUInt32 outputLen = 0;
    time_t raw_time= time(NULL);
    struct tm *time_info;
    char time_string[100];
    char filename[150];
    time_info = localtime(&raw_time); // 转换为本地时间结构体
    FILE* raw_fp=NULL;
    FILE* aec_fp=NULL;
    const char *folder_name = "ai_audio";  // 要检查的文件夹名
    strftime(time_string, sizeof(time_string), "%Y-%m-%d_%H-%M-%S", time_info);
    if(web_config->save_raw_audio|| web_config->save_aec_audio)
    {
        
        if (mkdir(folder_name, 0755) == 0) {
            quec_ai_log(LOG_AI_INFO,"文件夹创建成功: %s\n", folder_name);
        } else if (errno != EEXIST) {
            quec_ai_log(LOG_AI_ERR,"创建文件夹失败: %s", strerror(errno));
        }
    }
    if(web_config->save_raw_audio)
    {
        snprintf(filename, sizeof(filename), "%s/raw_%s.pcm",folder_name, time_string);
        raw_fp=fopen(filename,"wb");
        if(raw_fp==NULL)
        {
            quec_ai_log(LOG_AI_ERR,"open raw file error");
        }
    }
    if(web_config->save_aec_audio)
    {
        memset(filename,0,sizeof(filename));
        snprintf(filename, sizeof(filename), "%s/aec_%s.pcm", folder_name,time_string);
        aec_fp=fopen(filename,"wb");
        if(aec_fp==NULL)
        {
            quec_ai_log(LOG_AI_ERR,"open aec file error");
        }
    }
    while(1)
    {
        if(web_config->save_raw_audio|| web_config->save_aec_audio)
        { 
            if (mkdir(folder_name, 0755) == 0) {
                quec_ai_log(LOG_AI_INFO,"文件夹创建成功: %s\n", folder_name);
            } else if (errno != EEXIST) {
                quec_ai_log(LOG_AI_ERR,"创建文件夹失败: %s", strerror(errno));
            }
        }
        pa_simple_read(monitor_pa, speaker_data, KWS_AUDIO_FRAME_LEN, &error);
        pa_simple_read(pulse_audio_read_handle, audio_data, KWS_AUDIO_FRAME_LEN, &error);
        if(web_config->save_raw_audio&&raw_fp)
        {
            fwrite(audio_data, 1, KWS_AUDIO_FRAME_LEN, raw_fp);
            fflush(raw_fp);
        }
        aec_ans_process(echo_pObj,(short*)audio_data,(short*)speaker_data,KWS_AUDIO_FRAME_LEN/2,(short*)output_data,&outputLen);
        callback_list_dispatch(list, &output_data, KWS_AUDIO_FRAME_LEN);
        if(web_config->save_aec_audio&&aec_fp)
        {
            fwrite(output_data, 1, KWS_AUDIO_FRAME_LEN, aec_fp);
            fflush(aec_fp);
        }
        // usleep(5000);
    }
}

static int set_thread_priority(pthread_t thread, int policy, int priority) {
    struct sched_param param;
    param.sched_priority = priority;
    return pthread_setschedparam(thread, policy, &param);
}


int read_audio_init()
{
    int error;
    callback_list_init(&read_callback_list);
    pa_sample_spec      audio_record_info;
    audio_record_info.format   = PA_SAMPLE_S16LE;  // 16位有符号整数，小端字节序
    audio_record_info.rate     = 16000 ;            //16000 采样率
    audio_record_info.channels = 1   ;              //1 通道
    pulse_audio_read_handle = pa_simple_new(NULL,"ai_record",PA_STREAM_RECORD,"regular0","ai_s_record",&audio_record_info,NULL,NULL,&error);
    // 打开监听流（捕获默认喇叭的输出）
    monitor_pa = pa_simple_new(
        NULL,                        // 默认服务器
        "AEC Monitor",               // 应用名称
        PA_STREAM_RECORD,            // 录音模式
        "low-latency0.monitor",      // 默认设备（监听源）
        "monitor_of_default",        // 监听默认喇叭
        &audio_record_info,          // 采样格式
        NULL,                        // 无通道映射
        NULL,                        // 无缓冲属性
        NULL                         // 无错误回调
    );
    if (!monitor_pa) {
        quec_ai_log(LOG_AI_ERR, "Failed to open monitor stream:\n");
        return -1;
    }
    if( pulse_audio_read_handle == NULL ){
        quec_ai_log(LOG_AI_ERR,"pulse_audio_read_start audio record failed!");
        return -1;
    }
    if (pthread_create(&read_pthread_id, NULL, read_audio_pthread, &read_callback_list) != 0) {
        quec_ai_log(LOG_AI_ERR,"create audio play pthread failed!");
        return -1;
    }
    if (set_thread_priority(read_pthread_id, SCHED_OTHER, 0) != 0) {
        quec_ai_log(LOG_AI_ERR,"set_thread_priority failed!");
    }
    pthread_detach(read_pthread_id);
    return 0;
}

 

int init_aec()
{
    echo_pObj = aec_ans_create("./../config/Exp009M060_16k_mdl-v1.0_10ms_32ms_simple.mnn");
    if(echo_pObj==NULL)
    {
        quec_ai_log(LOG_AI_INFO,"echo creat error\n");
        return -1;
    }
    qlStatus err = aec_ans_init(echo_pObj);
    if(err!=QUEC_ANS_OK)
    {
        quec_ai_log(LOG_AI_INFO,"echo init error\n");
        return -1;
    }

    err = aec_ans_set_denoise_strength(echo_pObj,web_config->temperature);
    if(err!=QUEC_ANS_OK)
    {
        quec_ai_log(LOG_AI_INFO,"echo set  strength error\n");
        return -1;
    }
    return 0;
}


static int read_file_config()
{
	FILE* file = NULL;
	char *json_str = NULL;
	cJSON *root = NULL;
	int use_default = 0;

	web_config=malloc(sizeof(Websocket_Config));
	if(web_config==NULL)
	{
		quec_ai_log(LOG_AI_ERR,"web_config malloc error");
		return -1;
	}
    file = fopen("./../config/web_self_config.json", "r");
    if (!file) {
        quec_ai_log(LOG_AI_ERR,"aec_config file open error");
        use_default = 1;
        goto exit;
    }
    // 读取文件内容
    if (fseek(file, 0, SEEK_END) != 0) {
        quec_ai_log(LOG_AI_ERR,"aec_config fseek end error");
        use_default = 1;
        goto exit;
    }
    long file_size = ftell(file);
    if (file_size < 0) {
        quec_ai_log(LOG_AI_ERR,"aec_config ftell error: %ld", file_size);
        use_default = 1;
        goto exit;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        quec_ai_log(LOG_AI_ERR,"aec_config fseek set error");
        use_default = 1;
        goto exit;
    }
    json_str = (char *)malloc((size_t)file_size + 1);
    if (json_str == NULL) {
        quec_ai_log(LOG_AI_ERR,"aec_config malloc error, file_size=%ld", file_size);
        use_default = 1;
        goto exit;
    }
    size_t elements_read = fread(json_str, 1, (size_t)file_size, file);
    if(elements_read!=(size_t)file_size)
    {
        quec_ai_log(LOG_AI_ERR," file read size error: %zu,%ld\n", elements_read,file_size);
        use_default = 1;
        goto exit;
    }
    json_str[(size_t)file_size] = '\0';
    // 解析 JSON
    root= cJSON_Parse(json_str);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            quec_ai_log(LOG_AI_ERR,"aec_config cJSON_Parse error");
        }
        use_default = 1;
        goto exit;
    }
    cJSON *temperature_item = cJSON_GetObjectItemCaseSensitive(root, "aec_proportion");
    if (cJSON_IsNumber(temperature_item)) {
        web_config->temperature = (float)temperature_item->valuedouble;
    }else{
		web_config->temperature=1.0;
	}
	cJSON *save_raw_item = cJSON_GetObjectItemCaseSensitive(root, "save_raw_audio");
    if (cJSON_IsNumber(save_raw_item)) {
        web_config->save_raw_audio = (int)save_raw_item->valueint;
    }else{
		web_config->save_raw_audio=0;
	}
	cJSON *save_aec_item = cJSON_GetObjectItemCaseSensitive(root, "save_aec_audio");
	if (cJSON_IsNumber(save_aec_item)) {
        web_config->save_aec_audio = (int)save_aec_item->valueint;
    }else{
		web_config->save_aec_audio=0;
	}
	quec_ai_log(LOG_AI_INFO,"quec ai config aec_proportion=%.2f, save raw flag=%d,save aec flag=%d",web_config->temperature,web_config->save_raw_audio,web_config->save_aec_audio);
exit:
    if (use_default) {
		web_config->temperature=1.0;
		web_config->save_raw_audio=0;
		web_config->save_aec_audio=0;
    }
    if (root) {
        cJSON_Delete(root);
    }
    free(json_str);
    if (file) {
        fclose(file);
    }
	return 0;
}


int quec_ai_audio_init()
{
    if(read_file_config()!=0)
	{
		return -1;
	}
    /* 初始化麦克风阵列 */
    maix_mic_array_init();
    if(init_aec())
    {
        quec_ai_log(LOG_AI_ERR,"init_aec error\n");
        return -2;
    }
    if(read_audio_init()!=0)
    {
        quec_ai_log(LOG_AI_ERR,"read_audio_init error\n");
        return -3;
    }
    int ret=quec_ai_audio_play_pthread_init();
    return ret;
}