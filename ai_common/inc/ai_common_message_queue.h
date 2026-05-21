/**
 * @file        ai_common_message_queue.h
 * @brief       消息队列模块头文件
 * @author      Smart AI Robot
 * @date        2026/02/03
 * @version     v1.0
 * @copyright   Copyright (c) 2026 Smart AI Robot
 * @license     MIT License
 * 
 * @details     该头文件定义了基于链表实现的线程安全消息队列接口。
 *              提供了消息的发送、接收、队列管理等基础功能，
 *              支持多线程环境下的异步消息传递。
 */

#ifndef __AI_COMMON_MESSAGE_QUEUE_H
#define __AI_COMMON_MESSAGE_QUEUE_H

#include <pthread.h>

/**
 * @brief   消息节点结构体
 * 
 * @details 用于构建消息队列链表的节点，包含消息内容指针
 *          和指向下一个节点的指针。
 */
typedef struct MessageNode {
    char* message;              /**< 消息内容字符串指针 */
    struct MessageNode* next;   /**< 指向下一个消息节点的指针 */
} MessageNode;

/**
 * @brief   消息队列结构体
 * 
 * @details 管理消息队列的运行时状态，包含头尾节点指针、
 *          互斥锁和条件变量。
 */
typedef struct {
    MessageNode* head;          /**< 消息队列头节点 */
    MessageNode* tail;          /**< 消息队列尾节点 */
    pthread_mutex_t mutex;      /**< 互斥锁，保护队列操作 */
    pthread_cond_t cond;        /**< 条件变量，用于阻塞等待 */
} MessageQueue;

/**
 * @brief   初始化消息队列
 * 
 * @details 该函数用于初始化消息队列，分配内存并初始化
 *          互斥锁和条件变量。
 * 
 * @param   mq  [inout] 指向消息队列的指针
 */
void mq_init(MessageQueue* mq);

/**
 * @brief   销毁消息队列
 * 
 * @details 该函数用于销毁消息队列，释放所有节点内存
 *          并销毁互斥锁和条件变量。
 * 
 * @param   mq  [inout] 指向消息队列的指针
 */
void mq_destroy(MessageQueue* mq);

/**
 * @brief   向消息队列中添加消息
 * 
 * @details 该函数用于向消息队列尾部添加一条新消息，
 *          该操作是线程安全的。
 * 
 * @param   mq      [inout] 指向消息队列的指针
 * @param   message [in]    要添加的消息字符串
 */
void mq_push(MessageQueue* mq, char* message);

/**
 * @brief   从消息队列中取出消息（阻塞等待）
 * 
 * @details 该函数用于从消息队列头部取出一条消息，
 *          如果队列为空则阻塞等待直到有新消息到达。
 *          该操作是线程安全的。
 * 
 * @param   mq  [inout] 指向消息队列的指针
 * @return  char* 取出的消息字符串指针，调用者负责释放内存
 */
char* mq_pop(MessageQueue* mq);

#endif // MESSAGE_QUEUE_H
