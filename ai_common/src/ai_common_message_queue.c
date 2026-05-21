#include "ai_common_message_queue.h"
#include <stdlib.h>
#include <string.h>

// 初始化消息队列
void mq_init(MessageQueue* mq) {
    mq->head = NULL;
    mq->tail = NULL;
    pthread_mutex_init(&mq->mutex, NULL);
    pthread_cond_init(&mq->cond, NULL);
}

// 销毁消息队列
void mq_destroy(MessageQueue* mq) {
    pthread_mutex_destroy(&mq->mutex);
    pthread_cond_destroy(&mq->cond);
}

// 向消息队列中添加消息
void mq_push(MessageQueue* mq, char* message) {
    MessageNode* new_node = (MessageNode*)malloc(sizeof(MessageNode));
    new_node->message = message;
    new_node->next = NULL;

    pthread_mutex_lock(&mq->mutex);

    if (mq->tail == NULL) {
        mq->head = mq->tail = new_node;
    } else {
        mq->tail->next = new_node;
        mq->tail = new_node;
    }

    pthread_cond_signal(&mq->cond);  // 唤醒等待的线程
    pthread_mutex_unlock(&mq->mutex);
}

// 从消息队列中取出消息（阻塞等待）
char* mq_pop(MessageQueue* mq) {
    pthread_mutex_lock(&mq->mutex);

    // 如果队列为空，等待
    while (mq->head == NULL) {
        pthread_cond_wait(&mq->cond, &mq->mutex);
    }

    // 取出队头消息
    MessageNode* node = mq->head;
    char* message = node->message;
    mq->head = node->next;

    if (mq->head == NULL) {
        mq->tail = NULL;
    }

    free(node);
    pthread_mutex_unlock(&mq->mutex);

    return message;
}
