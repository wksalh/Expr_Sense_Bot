#ifndef CALLBACK_LIST_H
#define CALLBACK_LIST_H

#include <pthread.h>

// 回调函数类型定义
typedef void (*DataCallback)(void* data, int data_len);

// 回调函数节点结构体
typedef struct CallbackNode {
    DataCallback callback;      // 回调函数指针
    struct CallbackNode* next;  // 指向下一个节点的指针
} CallbackNode;

// 回调函数列表结构体
typedef struct {
    CallbackNode* head;         // 回调函数链表头节点
    pthread_mutex_t mutex;      // 链表操作互斥锁
} CallbackList;

// 初始化回调函数列表
void callback_list_init(CallbackList* list);

// 注册回调函数
void callback_list_register(CallbackList* list, DataCallback callback);

// 移除回调函数
void callback_list_unregister(CallbackList* list, DataCallback callback);

// 分发数据到所有回调函数
void callback_list_dispatch(CallbackList* list, void* data, int data_len);

// 销毁回调函数列表
void callback_list_destroy(CallbackList* list);

#endif // CALLBACK_LIST_H
