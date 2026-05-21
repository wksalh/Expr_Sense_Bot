/**
 * @file        ai_common_shm.h
 * @brief       共享内存通信模块头文件
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该头文件定义了基于POSIX共享内存的进程间通信接口。
 *              实现了TTLV格式数据（action、times、valid）的封装和传输，
 *              支持ROS2和Websocket两个子系统之间的数据共享。
 */

#ifndef __AI_COMMON_SHM_H__
#define __AI_COMMON_SHM_H__
#include <mqueue.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief   TTLV缓存数据结构体
 * 
 * @details 该结构体用于存储TTLV格式的缓存数据，
 *          包含动作类型、执行次数和数据有效性标志。
 */
 typedef struct 
 {
     int32_t action;        /**< ID1: action枚举值，标识动作类型 */
     int32_t times;         /**< ID2: 执行时间或执行次数 */
     bool valid;            /**< 数据是否有效标志 */
 } TtlvCacheData_t;

 /**
 * @brief   共享数据结构体
 * 
 * @details 该结构体定义了共享内存中存储的所有数据，
 *          包括数据就绪标志、TTLV缓存数据（ROS2和Websocket），
 *          以及额外的通用数据缓冲区。
 */
 typedef struct 
 {
     bool data_ready;        /**< 数据就绪标志 */
     TtlvCacheData_t ttlv_data;          /**< TTLV缓存数据（包含action, times, valid） */
     char data[1024];        /**< 额外数据缓冲区 */
 } SharedData;

 /**
 * @brief   POSIX共享内存管理器结构体
 * 
 * @details 该结构体管理共享内存的所有资源，
 *          包括文件描述符、数据指针和信号量。
 */
 typedef struct 
 {
    int shm_fd;             /**< 共享内存文件描述符 */
    SharedData* data;       /**< 共享数据指针 */
    sem_t* semaphore_websocket;       /**< Websocket命名信号量指针 */
    sem_t* semaphore_ros2;  /**< ROS2信号量指针 */
 } PosixShmManager;

/**
 * @brief   初始化共享内存IPC
 * 
 * @details 该函数用于初始化POSIX共享内存和信号量，
 *          创建或打开指定的共享内存区域。
 * 
 * @param   shm_name    [in] 共享内存名称
 * @return  int 成功返回0，失败返回负值错误码
 *          - 0: 初始化成功
 *          - -1: 共享内存创建失败
 *          - -2: 信号量初始化失败
 *          - -3: 内存映射失败
 */
int init_shared_memory_ipc(const char* shm_name);

/**
 * @brief   清理IPC资源
 * 
 * @details 该函数用于释放所有IPC相关资源，
 *          包括共享内存和信号量。
 */
void cleanup_ipc_resources();

/**
 * @brief   截取屏幕截图
 * 
 * @details 该函数用于捕获当前屏幕状态，
 *          返回截图数据的路径或数据指针。
 * 
 * @return  char* 截图文件路径或数据指针，失败返回NULL
 */
char *take_screenshot();

/**
 * @brief   发送TTLV数据到ROS2
 * 
 * @details 该函数用于将TTLV格式的数据通过共享内存
 *          发送给ROS2子系统。
 * 
 * @param   action  [in] 动作类型标识
 * @param   times   [in] 执行时间或次数
 * @param   valid   [in] 数据有效性标志
 * @return  int 成功返回0，失败返回负值错误码
 *          - 0: 发送成功
 *          - -1: 共享内存未初始化
 *          - -2: 发送失败
 */
int send_ttlv_data_to_ros2(int32_t action, int32_t times, bool valid);

#endif //__QUEC_AI_SHM_H__
