/**
 * @file        ai_audio_play.c
 * @brief       音频播放模块实现
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该文件实现了音频播放功能，包括本地音频播放和在线音频流播放。
 */

#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sndfile.h>
#include <pulse/simple.h>
#include <sys/resource.h>
#include <pulse/error.h>
#include <signal.h>
#include <errno.h>
#include "ai_audio_play.h"
#include "ai_log.h"
#include <time.h>
#include <semaphore.h>
#include "ai_common_message_queue.h"
#include "ai_common_ringbuffer.h"
#include "ai_audio.h"

static RingBuffer* online_aduio_buffer;
static pa_simple* audio_play_pa=NULL;
// 保护 audio_play_pa 流对象创建和访问的互斥锁
static pthread_mutex_t g_stream_mutex = PTHREAD_MUTEX_INITIALIZER;
//离在线播放互斥锁
static pthread_mutex_t play_data_mutex = PTHREAD_MUTEX_INITIALIZER;
static MessageQueue paly_audio_mq;
static bool pthread_play_flag=true;
static pthread_t thread_paly_audio,online_play_pthread_id;

static void local_play_audio(char* message)
{
    int err;
    FILE *file = fopen(message, "rb");
    if (!file) {
        quec_ai_log(LOG_AI_INFO,"文件打开失败 %s\n", message);
        return ;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        quec_ai_log(LOG_AI_INFO,"audio fseek end error\n");
        fclose(file);
        return ;
    }
    long file_size = ftell(file);
    if (file_size < 0) {
        quec_ai_log(LOG_AI_INFO,"audio ftell error: %ld\n", file_size);
        fclose(file);
        return ;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        quec_ai_log(LOG_AI_INFO,"audio fseek set error\n");
        fclose(file);
        return ;
    }
    size_t g_audio_size = (size_t)file_size;
    char* g_audio_data = (char*)malloc(g_audio_size+1);
    if (!g_audio_data) {
        quec_ai_log(LOG_AI_INFO,"audio malloc error\n");
        fclose(file);
        return ;
    }
    size_t read_count =fread(g_audio_data, 1, g_audio_size, file);
	if(read_count!=g_audio_size)
	{
		quec_ai_log(LOG_AI_INFO, "read file data error\n");
	}
    fclose(file);
    pthread_mutex_lock(&g_stream_mutex);
    if(audio_play_pa)
    {
        pthread_mutex_lock(&play_data_mutex);
        pa_simple_write(audio_play_pa,g_audio_data,read_count,&err);
        pthread_mutex_unlock(&play_data_mutex);
    }
    pthread_mutex_unlock(&g_stream_mutex);
    if (g_audio_data) {
        free(g_audio_data);
    }
}

void* play_audio_pthread(void* arg)
{
    (void)arg;
    while(pthread_play_flag)
    {
        char* message = mq_pop(&paly_audio_mq);  // 阻塞等待消息
            if (message == NULL) {
                continue;  // 如果消息为空，继续等待
            }
            quec_ai_log(LOG_AI_INFO,"play_audio_pthread received message: %s", message);
        local_play_audio(message);
        free(message);  // 释放消息内存
    }
    return NULL;
}
void* online_play_audio_pthread()
{
    char buffer[AUDIO_FRAME_LEN];
    int err;
     while(pthread_play_flag)
    {
        ringbuffer_read_wait(online_aduio_buffer,buffer,AUDIO_FRAME_LEN);
        pthread_mutex_lock(&g_stream_mutex);
        if(audio_play_pa)
        {
            pthread_mutex_lock(&play_data_mutex);
            pa_simple_write(audio_play_pa,buffer,AUDIO_FRAME_LEN,&err);
            pthread_mutex_unlock(&play_data_mutex);
        }
        pthread_mutex_unlock(&g_stream_mutex);
    }
    return NULL;
}

void quec_ai_local_play_audio(char* filename)
{
    char* filename_copy = strdup(filename);  // 复制字符串，确保内存安全
    mq_push(&paly_audio_mq,filename_copy);
}


int quec_ai_audio_play_open_stream(void){
    
    pthread_mutex_lock(&g_stream_mutex);
    // 双重检查：在加锁后再次检查，防止多个线程同时创建流对象
    if (audio_play_pa) {
        pthread_mutex_unlock(&g_stream_mutex);
        return 0;  // 流已存在
    }
    int error = 0;
    pa_sample_spec      audio_play_info;
    audio_play_info.format   = PA_SAMPLE_S16LE;
    audio_play_info.rate     = 16000;
    audio_play_info.channels = 1;
    
    // 【关键修改】设置较小的缓冲区大小，减少延迟，提高打断响应速度
    // 通过设置较小的 tlength（目标长度），可以减少硬件缓冲区中的数据量
    // 计算：16000 采样率 * 2 字节/采样 * 0.05 秒 = 1600 字节（约50毫秒的音频数据）
    pa_buffer_attr buffer_attr;
    uint32_t target_bytes = 16000 * 2 * 0.05;  // 50毫秒的音频数据
    buffer_attr.maxlength = (uint32_t) -1;  // 最大长度（使用默认值）
    buffer_attr.tlength = target_bytes;     // 目标长度：50毫秒的音频数据
    buffer_attr.prebuf = (uint32_t) -1;     // 预缓冲（使用默认值）
    buffer_attr.minreq = (uint32_t) -1;     // 最小请求（使用默认值）
    buffer_attr.fragsize = (uint32_t) -1;  // 片段大小（使用默认值）
    
    audio_play_pa = pa_simple_new(NULL,"ai_play",PA_STREAM_PLAYBACK,NULL,"ai_s_play",&audio_play_info,NULL,&buffer_attr,&error);
    if (audio_play_pa == NULL) {
        quec_ai_log(LOG_AI_ERR,"pa_simple_new audio play failed: %d(%s)\n",error,pa_strerror(error));
        pthread_mutex_unlock(&g_stream_mutex);
        return -1;
    }
    // 先释放流对象锁，再设置 is_playing（避免死锁）
    pthread_mutex_unlock(&g_stream_mutex);
    return 0;
}

int quec_ai_audio_play(char *data,int data_len){
    if (data == NULL || data_len <= 0) {
        quec_ai_log(LOG_AI_INFO, "quec_ai_audio_play: invalid parameters\n");
        return -1;
    }
    pthread_mutex_lock(&g_stream_mutex);
    if(audio_play_pa)
    {
        ringbuffer_write(online_aduio_buffer,data,data_len);
    }
    pthread_mutex_unlock(&g_stream_mutex);
    return 0;
}

void quec_ai_audio_play_flush(void)
{
    ringbuffer_clear(online_aduio_buffer);
}

void quec_ai_audio_play_pthread_destroy(void){
    // 【关键修改】使用互斥锁保护流对象的访问和销毁
    pthread_mutex_lock(&g_stream_mutex);
    pa_simple *stream = audio_play_pa;
    audio_play_pa = NULL;
    pthread_mutex_unlock(&g_stream_mutex);
    if (stream) {
        pa_simple_free(stream);
    }
    if(online_aduio_buffer)
    {
        ringbuffer_destroy(online_aduio_buffer);
    }
    return;
}

int quec_ai_audio_play_pthread_init(void){
    mq_init(&paly_audio_mq);
    online_aduio_buffer=ringbuffer_init(PLAY_RING_BUFFER_SIZE);
    
    if (pthread_create(&thread_paly_audio, NULL, play_audio_pthread, NULL) != 0) {
        quec_ai_log(LOG_AI_INFO,"create audio play pthread failed!");
        return -1;
    }
    if (pthread_create(&online_play_pthread_id, NULL, online_play_audio_pthread, NULL) != 0) {
        quec_ai_log(LOG_AI_INFO,"create online audio play pthread failed!");
        return -1;
    }
    return quec_ai_audio_play_open_stream();
}
