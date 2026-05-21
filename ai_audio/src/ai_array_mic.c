/**
 * @file        ai_array_mic.c
 * @brief       麦克风阵列模块实现
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该文件实现了麦克风阵列功能，包括DOA估计、声源定位等。
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <fftw3.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <fftw3.h>
#include "ai_common_ringbuffer.h"
#include "ai_log.h"
#include "ai_common_shm.h"


#define NUM_MICS 6           // 实际麦克风数量
#define NUM_CHANNELS 8       // 音频通道总数
#define NUM_SAMPLES 480   //实际每次读取的音频长度，
// #define MIC_DEAL_SAMPLES 16000
#define SOUND_SPEED 343.0f
#define MIC_DISTANCE 0.035f
#define SEARCH_RESOLUTION 0.5f  // 角度搜索分辨率0.5度

#define MIXCAR_DEAL_TIME    2000   //2S的时间

#define MIXCAR_RINGBUFFER_SIZE 10*16000*8*2  //10s的buffer的数据长度
#define MIXCAR_RINGBUFFER_SIZE_READ MIXCAR_DEAL_TIME*16*8*2  //真正处理时只要2s的数据长度

static bool mic_done=false;
static pa_simple* maix_mic_array_handle = NULL;
static pthread_t  mic_array_pthread_id;
static bool maixmic_deal_flag=false;

extern struct timeval maix_mic_end_time;

static RingBuffer *Mixcar_Aduio_buffer;

// 麦克风布局：6个麦克风，不均匀分布
static float mic_angles[NUM_MICS] = {0.0f, 60.0f, 120.0f, 180.0f, 240.0f, 300.0f};
static float mic_positions[NUM_MICS][2];

// 通道映射：音频通道 -> 麦克风索引
// 通道0-7对应数组索引0-7，但只有6个有效麦克风
static int channel_to_mic[NUM_CHANNELS] = {
    0,   // 通道1 -> mic1 (0°)
    1,   // 通道2 -> mic2 (60°)
    4,   // 通道3 -> mic5 (240°)
    5,   // 通道4 -> mic6 (300°)
    -1,  // 通道5 -> 无效
    -1,  // 通道6 -> 无效
    2,   // 通道7 -> mic3 (120°)
    3    // 通道8 -> mic4 (180°)
};

// 初始化麦克风位置
void init_mic_positions() {
    for (int i = 0; i < NUM_MICS; i++) {
        float rad = mic_angles[i] * M_PI / 180.0f;
        mic_positions[i][0] = MIC_DISTANCE * cosf(rad);
        mic_positions[i][1] = MIC_DISTANCE * sinf(rad);
    }
}

// 读取PCM文件
int16_t* read_pcm_file(const char* filename, size_t* num_samples) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        quec_ai_log(LOG_AI_ERR,"无法打开文件 %s\n", filename);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        quec_ai_log(LOG_AI_ERR,"文件定位失败 %s\n", filename);
        fclose(file);
        return NULL;
    }
    long file_size_long = ftell(file);
    if (file_size_long <= 0) {
        quec_ai_log(LOG_AI_ERR,"文件大小异常 %s: %ld\n", filename, file_size_long);
        fclose(file);
        return NULL;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        quec_ai_log(LOG_AI_ERR,"文件定位失败 %s\n", filename);
        fclose(file);
        return NULL;
    }
    size_t file_size = (size_t)file_size_long;

    size_t total_samples = file_size / (NUM_CHANNELS * 2);

    char* raw_data = (char*)malloc(file_size);
    int16_t* audio_data = (int16_t*)malloc(total_samples * NUM_CHANNELS * sizeof(int16_t));

    if (!raw_data || !audio_data) {
        quec_ai_log(LOG_AI_ERR, "内存分配失败\n");
        if (raw_data) free(raw_data);
        if (audio_data) free(audio_data);
        fclose(file);
        return NULL;
    }

    size_t read_bytes = fread(raw_data, 1, file_size, file);
    fclose(file);

    if (read_bytes != file_size) {
        quec_ai_log(LOG_AI_ERR,"警告: 只读取了 %zu 字节，期望 %zu 字节\n", read_bytes, file_size);
        total_samples = read_bytes / (NUM_CHANNELS * 2);
    }

    // 转换为int16_t
    for (size_t i = 0; i < total_samples * NUM_CHANNELS; i++) {
        int16_t sample = (int16_t)((unsigned char)raw_data[2*i] |
                                   ((unsigned char)raw_data[2*i+1] << 8));
        // 防止溢出
        if (sample == -32768) sample = -32767;
        audio_data[i] = sample;
    }

    free(raw_data);

    if (num_samples) {
        *num_samples = total_samples;
    }

    quec_ai_log(LOG_AI_INFO,"读取 %s: %zu 样本\n", filename, total_samples);
    return audio_data;
}

// 分离有效麦克风数据
void separate_mic_data(int16_t* audio_data, size_t num_samples, 
                       int16_t** mic_data, float* mic_energy) {
    for (int mic = 0; mic < NUM_MICS; mic++) {
        mic_data[mic] = (int16_t*)malloc(num_samples * sizeof(int16_t));
        mic_energy[mic] = 0.0f;
        
        // 找到对应的音频通道
        int channel = -1;
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            if (channel_to_mic[ch] == mic) {
                channel = ch;
                break;
            }
        }
        
        if (channel == -1) {
           quec_ai_log(LOG_AI_INFO,"错误: 未找到麦克风 %d 对应的通道\n", mic);
            continue;
        }
        
        // 提取数据并计算能量
        for (size_t i = 0; i < num_samples; i++) {
            int16_t sample = audio_data[i * NUM_CHANNELS + channel];
            mic_data[mic][i] = sample;
            mic_energy[mic] += abs(sample);
        }
        mic_energy[mic] /= num_samples;
        
        quec_ai_log(LOG_AI_INFO,"  麦克风 %d (%.0f°): 能量=%.2f\n", 
               mic+1, mic_angles[mic], mic_energy[mic]);
    }
}

// 计算理论时延（精确版本）
float calculate_time_delay_exact(int mic1, int mic2, float azimuth_deg) {
    float azimuth_rad = azimuth_deg * M_PI / 180.0f;
    
    // 计算方向向量
    float dx = cosf(azimuth_rad);
    float dy = sinf(azimuth_rad);
    
    // 计算两个麦克风在方向上的投影差
    float proj1 = mic_positions[mic1][0] * dx + mic_positions[mic1][1] * dy;
    float proj2 = mic_positions[mic2][0] * dx + mic_positions[mic2][1] * dy;
    
    return (proj1 - proj2) / SOUND_SPEED;
}

// 基于互相关的简单DOA估计（用于快速定位）
float estimate_doa_simple(int16_t** mic_data, size_t num_samples, float* mic_energy) {
    
    // 找到能量最大的麦克风
    int max_mic = 0;
    float max_energy = mic_energy[0];
    for (int i = 1; i < NUM_MICS; i++) {
        if (mic_energy[i] > max_energy) {
            max_energy = mic_energy[i];
            max_mic = i;
        }
    }
    
    quec_ai_log(LOG_AI_INFO,"  最大能量麦克风: Mic%d (%.0f°), 能量=%.2f\n", 
           max_mic, mic_angles[max_mic], max_energy);
    
    // 如果只有一个麦克风有显著能量，直接返回其方向
    float second_max = 0;
    for (int i = 0; i < NUM_MICS; i++) {
        if (i != max_mic && mic_energy[i] > second_max) {
            second_max = mic_energy[i];
        }
    }
    
    if (max_energy > second_max * 5.0f) {
        quec_ai_log(LOG_AI_INFO,"  单麦克风占优，直接返回方向: %.1f°\n", mic_angles[max_mic]);
        return mic_angles[max_mic];
    }
    
    // 否则，使用互相关方法
    float best_angle = 0;
    float best_correlation = -1e9;
    
    // 搜索所有角度（1度分辨率）
    for (float angle = 0; angle < 360; angle += 1.0f) {
        float total_corr = 0;
        int valid_pairs = 0;
        
        // 计算所有麦克风对的互相关
        for (int i = 0; i < NUM_MICS; i++) {
            for (int j = i+1; j < NUM_MICS; j++) {
                // 计算理论时延
                float tau = calculate_time_delay_exact(i, j, angle);
                int delay = (int)(tau * 16000.0f + 0.5f);
                
                // 限制延迟范围
                if (abs(delay) > num_samples/4) continue;
                
                // 计算互相关
                float corr = 0;
                int start = delay > 0 ? delay : 0;
                int end = num_samples - (delay < 0 ? -delay : 0);
                int step = 5; // 降低计算量
                
                for (int k = start; k < end; k += step) {
                    int16_t s1 = mic_data[i][k];
                    int16_t s2 = mic_data[j][k - delay];
                    corr += s1 * s2;
                }
                
                total_corr += corr;
                valid_pairs++;
            }
        }
        
        if (valid_pairs > 0) {
            float avg_corr = total_corr / valid_pairs;
            if (avg_corr > best_correlation) {
                best_correlation = avg_corr;
                best_angle = angle;
            }
        }
    }
    
    quec_ai_log(LOG_AI_INFO,"  互相关估计方向: %.1f°, 相关性=%.2f\n", best_angle, best_correlation);
    return best_angle;
}

// 精确DOA估计（基于GCC-PHAT和二次插值）
float estimate_doa_precise(int16_t** mic_data, size_t num_samples) {
    quec_ai_log(LOG_AI_INFO,"\n精确DOA估计（GCC-PHAT + 二次插值）:\n");
    
    // 初始化FFTW
    fftw_complex* fft_data[NUM_MICS];
    double* fft_input[NUM_MICS];
    fftw_plan fft_plans[NUM_MICS];
    
    for (int i = 0; i < NUM_MICS; i++) {
        fft_input[i] = (double*)fftw_malloc(num_samples * sizeof(double));
        fft_data[i] = (fftw_complex*)fftw_malloc((num_samples/2 + 1) * sizeof(fftw_complex));
        fft_plans[i] = fftw_plan_dft_r2c_1d(num_samples, fft_input[i], fft_data[i], FFTW_ESTIMATE);
        
        // 填充输入数据（应用汉宁窗）
        for (size_t j = 0; j < num_samples; j++) {
            double window = 0.5 * (1.0 - cos(2.0 * M_PI * j / (num_samples - 1)));
            fft_input[i][j] = mic_data[i][j] * window;
        }
        
        fftw_execute(fft_plans[i]);
    }
    
    // 粗搜索：1度分辨率
    float coarse_angle = 0;
    float coarse_energy = -1e9;
    float energy_profile[360] = {0};
    
    for (float angle = 0; angle < 360; angle += 1.0f) {
        float total_energy = 0;
        int pair_count = 0;
        
        for (int i = 0; i < NUM_MICS; i++) {
            for (int j = i+1; j < NUM_MICS; j++) {
                // 计算理论时延
                float tau = calculate_time_delay_exact(i, j, angle);
                float phase_shift = tau * 16000.0f * 2 * M_PI;
                
                float corr_real = 0, corr_imag = 0;
                
                // GCC-PHAT计算
                for (size_t k = 1; k < num_samples/2; k++) {
                    float phase = k * phase_shift / num_samples;
                    
                    float real1 = fft_data[i][k][0];
                    float imag1 = fft_data[i][k][1];
                    float real2 = fft_data[j][k][0];
                    float imag2 = fft_data[j][k][1];
                    
                    // 互功率谱
                    float cross_real = real1 * real2 + imag1 * imag2;
                    float cross_imag = real1 * imag2 - imag1 * real2;
                    
                    // PHAT加权
                    float magnitude = sqrt(cross_real * cross_real + cross_imag * cross_imag);
                    if (magnitude > 1e-10) {
                        cross_real /= magnitude;
                        cross_imag /= magnitude;
                    }
                    
                    // 应用相位差
                    float cos_phase = cosf(phase);
                    float sin_phase = sinf(phase);
                    
                    corr_real += cross_real * cos_phase - cross_imag * sin_phase;
                    corr_imag += cross_real * sin_phase + cross_imag * cos_phase;
                }
                
                float corr_mag = sqrt(corr_real * corr_real + corr_imag * corr_imag);
                total_energy += corr_mag;
                pair_count++;
            }
        }
        
        if (pair_count > 0) {
            float avg_energy = total_energy / pair_count;
            energy_profile[(int)angle] = avg_energy;
            
            if (avg_energy > coarse_energy) {
                coarse_energy = avg_energy;
                coarse_angle = angle;
            }
        }
    }
    
    // 细搜索：在粗搜索结果附近进行0.1度分辨率搜索
    float fine_angle = coarse_angle;
    float fine_energy = coarse_energy;
    
    float search_start = coarse_angle - 2.0f;
    float search_end = coarse_angle + 2.0f;
    
    for (float angle = search_start; angle <= search_end; angle += 0.1f) {
        float wrapped_angle = angle;
        while (wrapped_angle < 0) wrapped_angle += 360;
        while (wrapped_angle >= 360) wrapped_angle -= 360;
        
        float total_energy = 0;
        int pair_count = 0;
        
        for (int i = 0; i < NUM_MICS; i++) {
            for (int j = i+1; j < NUM_MICS; j++) {
                float tau = calculate_time_delay_exact(i, j, wrapped_angle);
                float phase_shift = tau * 16000.0f * 2 * M_PI;
                
                float corr_real = 0, corr_imag = 0;
                
                // 只计算关键频段（500-4000Hz，人声主要频段）
                int low_k = 500 * num_samples / 16000;
                int high_k = 4000 * num_samples / 16000;
                
                for (int k = low_k; k <= high_k && k < (int)(num_samples/2); k++) {
                    float phase = k * phase_shift / num_samples;
                    
                    float real1 = fft_data[i][k][0];
                    float imag1 = fft_data[i][k][1];
                    float real2 = fft_data[j][k][0];
                    float imag2 = fft_data[j][k][1];
                    
                    float cross_real = real1 * real2 + imag1 * imag2;
                    float cross_imag = real1 * imag2 - imag1 * real2;
                    
                    float magnitude = sqrt(cross_real * cross_real + cross_imag * cross_imag);
                    if (magnitude > 1e-10) {
                        cross_real /= magnitude;
                        cross_imag /= magnitude;
                    }
                    
                    float cos_phase = cosf(phase);
                    float sin_phase = sinf(phase);
                    
                    corr_real += cross_real * cos_phase - cross_imag * sin_phase;
                    corr_imag += cross_real * sin_phase + cross_imag * cos_phase;
                }
                
                float corr_mag = sqrt(corr_real * corr_real + corr_imag * corr_imag);
                total_energy += corr_mag;
                pair_count++;
            }
        }
        
        if (pair_count > 0) {
            float avg_energy = total_energy / pair_count;
            if (avg_energy > fine_energy) {
                fine_energy = avg_energy;
                fine_angle = wrapped_angle;
            }
        }
    }
    
    // 二次插值提高精度
    float prev_angle = fine_angle - 0.1f;
    if (prev_angle < 0) prev_angle += 360;
    float next_angle = fine_angle + 0.1f;
    if (next_angle >= 360) next_angle -= 360;
    
    float y0 = energy_profile[(int)prev_angle];
    float y1 = energy_profile[(int)fine_angle];
    float y2 = energy_profile[(int)next_angle];
    
    // 二次插值公式
    float refined_angle = fine_angle;
    if (fabs(y0 - 2*y1 + y2) > 1e-6) {
        refined_angle = fine_angle + 0.5f * (y0 - y2) / (y0 - 2*y1 + y2) * 0.1f;
    }
    
    // 确保角度在0-360范围内
    while (refined_angle < 0) refined_angle += 360;
    while (refined_angle >= 360) refined_angle -= 360;
    
    // 清理FFTW资源
    for (int i = 0; i < NUM_MICS; i++) {
        fftw_destroy_plan(fft_plans[i]);
        fftw_free(fft_input[i]);
        fftw_free(fft_data[i]);
    }
    return refined_angle;
}

// /**
//  * 将声源角度转换为单位圆上的坐标（不考虑距离）
//  * @param angle_deg 声源方向角度 (0°为正东/X轴正方向)
//  * @param out_x     输出的X坐标 (-1.0 到 1.0)
//  * @param out_y     输出的Y坐标 (-1.0 到 1.0)
//  */
// static void angle_to_unit_circle(float angle_deg, float* out_x, float* out_y) {
//     // 将角度从度转换为弧度
//     float angle_rad = angle_deg * M_PI / 180.0f;
    
//     // 计算单位圆上的坐标
//     *out_x = cosf(angle_rad);
//     *out_y = sinf(angle_rad);
// }

void set_mixcar_deal()
{
    maixmic_deal_flag=true;
}

void char_to_int16(char *char_data, int16_t *int16_data, size_t num_samples) {
    for (size_t i = 0; i < num_samples; i++) {
        int16_data[i] = (int16_t)((char_data[2*i+1] << 8) | (char_data[2*i] & 0xFF));
    }
}

// 音频读取线程
void* maix_mic_array_read_pthread(void* arg) {
    (void)arg;
    const size_t audio_data_size = NUM_CHANNELS * NUM_SAMPLES * 2;
    const size_t audio_deal_size = MIXCAR_RINGBUFFER_SIZE_READ;
    char *audio_data_char = (char*)malloc(audio_data_size);
    char *audio_deal_char = (char*)malloc(audio_deal_size);
    int16_t *audio_deal_int16 = (int16_t*)malloc((audio_deal_size / 2) * sizeof(int16_t));
    struct timeval maix_mic_time_end;
    int discard_data_size=0;

    if (!audio_data_char || !audio_deal_char || !audio_deal_int16) {
        quec_ai_log(LOG_AI_ERR, "maix mic audio buffer malloc failed\n");
        free(audio_data_char);
        free(audio_deal_char);
        free(audio_deal_int16);
        return NULL;
    }

    while (!mic_done) {
        int error;
        if (pa_simple_read(maix_mic_array_handle, audio_data_char, audio_data_size, &error) < 0) {
            quec_ai_log(LOG_AI_ERR, "[ERROR] pa_simple_read failed: %s\n", pa_strerror(error));
            continue;
        }
        ringbuffer_write(Mixcar_Aduio_buffer,audio_data_char, audio_data_size);
        if(maixmic_deal_flag)
        {
            maixmic_deal_flag=false;
            /*丢弃数据*/
            /*数据总长度*/
            int buffer_all_size=ringbuffer_used(Mixcar_Aduio_buffer);
            gettimeofday(&maix_mic_time_end, NULL);
            long seconds = maix_mic_time_end.tv_sec - maix_mic_end_time.tv_sec;
            long microseconds = maix_mic_time_end.tv_usec - maix_mic_end_time.tv_usec;
            long elapsed_microseconds = seconds * 1000000 + microseconds;

            // 转换为毫秒
            int elapsed_milliseconds = (int)(elapsed_microseconds / 1000.0);
            int read_all_size = (elapsed_milliseconds+MIXCAR_DEAL_TIME)*8*2*16;
            if(buffer_all_size>read_all_size)  //丢弃前面的buffer音频数据
            {
                discard_data_size=buffer_all_size-read_all_size;
                char* discard_data=(char*)malloc(discard_data_size);
                if (discard_data == NULL) {
                    quec_ai_log(LOG_AI_ERR,"discard data malloc failed, size=%d\n", discard_data_size);
                    ringbuffer_clear(Mixcar_Aduio_buffer);
                    continue;
                }
                ringbuffer_read(Mixcar_Aduio_buffer, discard_data, discard_data_size);
                free(discard_data);
                quec_ai_log(LOG_AI_INFO,"discard data ms= %d,%d\n",discard_data_size/16/8/2,elapsed_milliseconds+MIXCAR_DEAL_TIME);
            }
            ringbuffer_read(Mixcar_Aduio_buffer, audio_deal_char, MIXCAR_RINGBUFFER_SIZE_READ);
            char_to_int16(audio_deal_char, audio_deal_int16, MIXCAR_RINGBUFFER_SIZE_READ/2);
            int16_t* mic_data[NUM_MICS];
            float mic_energy[NUM_MICS];
            separate_mic_data(audio_deal_int16, MIXCAR_RINGBUFFER_SIZE_READ/8/2, mic_data, mic_energy);
            // 快速DOA估计
            float fast_angle = estimate_doa_simple(mic_data, MIXCAR_RINGBUFFER_SIZE_READ/8/2, mic_energy);
            // 精确DOA估计
            float precise_angle = estimate_doa_precise(mic_data, MIXCAR_RINGBUFFER_SIZE_READ/8/2);
            // 找出能量最大的两个麦克风
            float first_max_energy = 0, second_max_energy = 0;
            int first_max_mic = 0;
            for (int i = 0; i < NUM_MICS; i++) {
                if (mic_energy[i] > first_max_energy) {
                    second_max_energy = first_max_energy;
                    first_max_energy = mic_energy[i];
                    first_max_mic = i;
                } else if (mic_energy[i] > second_max_energy) {
                    second_max_energy = mic_energy[i];
                }
            }
            // 能量比阈值判断
            float energy_ratio = (second_max_energy > 0.1f) ?
                                (first_max_energy / second_max_energy) : 100.0f;
            // 智能选择最终角度
            float final_angle;
            const float ENERGY_THRESHOLD = 3.0f;  // 能量比阈值，可调整

            if (energy_ratio > ENERGY_THRESHOLD) {
                // 能量差异显著，采用最大能量麦克风的固定角度
                final_angle = mic_angles[first_max_mic];
            } else {
                // 能量分布较均匀，采用GCC-PHAT精确计算结果
                final_angle = precise_angle;
            }
            // 新增：转换为单位圆坐标
            // float coord_x, coord_y;
            // angle_to_unit_circle(final_angle, &coord_x, &coord_y);
            // quec_ai_log(LOG_AI_INFO,"单位圆坐标: (%.3f, %.3f)\n", coord_x, coord_y);
            quec_ai_log(LOG_AI_INFO,"快速估计: %.1f°,精确估计: %.2f°,最大能量麦克风方向: %.0f°,最终选定角度: %.2f°",fast_angle,precise_angle,mic_angles[first_max_mic],final_angle);
            // int step_action=((int)final_angle)%30;
            send_ttlv_data_to_ros2(19, (int)final_angle, true);


            // 清理
            for (int i = 0; i < NUM_MICS; i++) {
                free(mic_data[i]);
            }
            ringbuffer_clear(Mixcar_Aduio_buffer);
        }

    }
    free(audio_data_char);
    free(audio_deal_char);
    free(audio_deal_int16);
    return NULL;
}

int has_mic_array() {
    FILE *fp = popen(" pactl list sources short", "r");
    if (fp == NULL) {
        quec_ai_log(LOG_AI_ERR,"Failed to execute command");
        return -1;
    }
    char buffer[256];
    int found = 0;
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (strstr(buffer, "alsa_input.0.multichannel-input") != NULL) {
            found = 1;
            break;
        }
    }
    pclose(fp);
    return found;
}

int load_pulseaudio_module() {
    int has_mic_ret=has_mic_array();
    quec_ai_log(LOG_AI_INFO,"has mic ret=%d\n",has_mic_ret);
    if(has_mic_ret == 1)
    {
        int ret = system("pactl load-module module-alsa-card device_id=0");
        if (ret != 0) {
            quec_ai_log(LOG_AI_ERR,"Failed to load PulseAudio module. Error: %d\n", ret);
            return -1;
        } else {
            quec_ai_log(LOG_AI_INFO,"PulseAudio module loaded successfully.\n");
            return 0;
        }
    }else{
        quec_ai_log(LOG_AI_INFO,"array mic not found.\n");
        return -1;
    }
}

// 初始化 PulseAudio
int maix_mic_array_init() {
    if(load_pulseaudio_module()!=0)
    {
        return -1;
    }
    init_mic_positions();
    Mixcar_Aduio_buffer=ringbuffer_init(MIXCAR_RINGBUFFER_SIZE);
    if(Mixcar_Aduio_buffer==NULL)
    {
        quec_ai_log(LOG_AI_ERR, "Mixcar ringbuffer init failed\n");
        return -1;
    }
    int error;
    pa_sample_spec audio_record_info = {
        .format = PA_SAMPLE_S16LE,
        .rate = 16000,
        .channels = 8
    };
    pa_buffer_attr buffer_attr = {
        .maxlength = (uint32_t)-1,  // 最大缓冲区长度（默认）
        .tlength = pa_usec_to_bytes(10 * 1000, &audio_record_info),  // 目标延迟：10ms
        .prebuf = (uint32_t)-1,     // 默认预缓冲
        .minreq = (uint32_t)-1      // 默认最小请求
    };
    maix_mic_array_handle = pa_simple_new(
        NULL, "maix_mic_record", PA_STREAM_RECORD,
        "alsa_input.0.multichannel-input", "maix_mic_s_record",
        &audio_record_info, NULL, &buffer_attr, &error
    );

    if (!maix_mic_array_handle) {
        quec_ai_log(LOG_AI_ERR, "[ERROR] pa_simple_new failed: %s\n", pa_strerror(error));
        ringbuffer_destroy(Mixcar_Aduio_buffer);
        Mixcar_Aduio_buffer = NULL;
        return -1;
    }
    if (pthread_create(&mic_array_pthread_id, NULL, maix_mic_array_read_pthread, NULL) != 0) {
        quec_ai_log(LOG_AI_ERR, "create maix mic pthread failed\n");
        pa_simple_free(maix_mic_array_handle);
        maix_mic_array_handle = NULL;
        ringbuffer_destroy(Mixcar_Aduio_buffer);
        Mixcar_Aduio_buffer = NULL;
        return -1;
    }
    return 0;
}