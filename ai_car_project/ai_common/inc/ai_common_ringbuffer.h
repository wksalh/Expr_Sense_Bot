/**
 * @file        ai_common_ringbuffer.h
 * @brief       环形缓冲区模块头文件
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该头文件定义了线程安全的环形缓冲区（RingBuffer）接口。
 *              提供了创建、销毁、读写、阻塞读取等完整功能，
 *              使用互斥锁和条件变量保证线程安全。
 *              适用于音频数据流、网络数据包等场景的数据缓冲。
 */

#ifndef __AI_COMMON_RINGBUFFER__
#define __AI_COMMON_RINGBUFFER__


#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   环形缓冲区结构体
 * 
 * @details 该结构体管理环形缓冲区的所有状态信息，
 *          包括数据缓冲区、读写指针、互斥锁和条件变量。
 */
typedef struct {
    unsigned char *buffer;  /**< 环形缓冲区数据存储区 */
    size_t size;            /**< 缓冲区总大小（字节） */
    size_t head;            /**< 写指针位置 */
    size_t tail;            /**< 读指针位置 */
    size_t count;           /**< 当前缓冲区中的数据量 */
    pthread_mutex_t mutex;  /**< 互斥锁，保护缓冲区操作 */
    pthread_cond_t cond;    /**< 条件变量，用于阻塞等待 */
} RingBuffer;

/**
 * @brief   创建环形缓冲区
 * 
 * @details 该函数用于创建一个指定大小的环形缓冲区，
 *          分配内存并初始化所有成员变量。
 * 
 * @param   size    [in] 缓冲区大小（字节数）
 * @return  RingBuffer* 成功返回环形缓冲区指针，失败返回NULL
 */
RingBuffer* ringbuffer_init(size_t size);

/**
 * @brief   销毁环形缓冲区
 * 
 * @details 该函数用于释放环形缓冲区的所有资源，
 *          包括缓冲区内存、互斥锁和条件变量。
 * 
 * @param   rb  [in] 指向环形缓冲区的指针
 */
void ringbuffer_destroy(RingBuffer *rb);

/**
 * @brief   往环形缓冲区写数据
 * 
 * @details 该函数用于将数据写入环形缓冲区，
 *          如果缓冲区空间不足则只写入部分数据。
 * 
 * @param   rb      [inout] 指向环形缓冲区的指针
 * @param   data    [in]    要写入的数据指针
 * @param   len     [in]    要写入的数据长度（字节数）
 * @return  size_t  实际写入的字节数
 */
size_t ringbuffer_write(RingBuffer *rb, char *data, size_t len);

/**
 * @brief   阻塞式读取环形缓冲区数据
 * 
 * @details 该函数用于从环形缓冲区读取指定长度的数据，
 *          如果数据不足则阻塞等待直到有足够数据。
 * 
 * @param   rb      [inout] 指向环形缓冲区的指针
 * @param   data    [out]   接收数据的缓冲区指针
 * @param   len     [in]    要读取的数据长度（字节数）
 * @return  size_t  实际读取的字节数
 */
size_t ringbuffer_read_wait(RingBuffer *rb, char *data, size_t len);

/**
 * @brief   获取环形缓冲区中可用数据量
 * 
 * @details 该函数用于查询当前缓冲区中可读取的数据量。
 * 
 * @param   rb  [in] 指向环形缓冲区的指针
 * @return  size_t 当前可读取的数据字节数
 */
size_t ringbuffer_used(RingBuffer *rb);

/**
 * @brief   读取环形缓冲区数据（非阻塞）
 * 
 * @details 该函数用于从环形缓冲区读取数据，
 *          如果数据不足则只读取可用部分。
 * 
 * @param   rb      [inout] 指向环形缓冲区的指针
 * @param   data    [out]   接收数据的缓冲区指针
 * @param   len     [in]    要读取的数据长度（字节数）
 * @return  size_t  实际读取的字节数
 */
size_t ringbuffer_read(RingBuffer *rb, char *data, size_t len);

/**
 * @brief   预览环形缓冲区数据（不取出）
 * 
 * @details 该函数用于预览环形缓冲区头部数据，
 *          读取后数据不会从缓冲区中移除。
 * 
 * @param   rb      [in] 指向环形缓冲区的指针
 * @param   data    [out] 接收数据的缓冲区指针
 * @param   len     [in] 要预览的数据长度（字节数）
 * @return  size_t  实际预览的字节数
 */
size_t ringbuffer_peek(RingBuffer *rb, char *data, size_t len);

/**
 * @brief   清空环形缓冲区
 * 
 * @details 该函数用于清空环形缓冲区中的所有数据，
 *          重置读写指针和数据计数。
 * 
 * @param   rb  [inout] 指向环形缓冲区的指针
 */
void ringbuffer_clear(RingBuffer *rb);

#ifdef __cplusplus
}
#endif

#endif
