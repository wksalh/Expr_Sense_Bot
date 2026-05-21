#include "ai_common_list.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// 初始化回调函数列表
void callback_list_init(CallbackList* list) {
    list->head = NULL;
    pthread_mutex_init(&list->mutex, NULL);
}

// 注册回调函数
void callback_list_register(CallbackList* list, DataCallback callback) {
    pthread_mutex_lock(&list->mutex);

    CallbackNode* new_node = (CallbackNode*)malloc(sizeof(CallbackNode));
    if (!new_node) {
        pthread_mutex_unlock(&list->mutex);
        return;
    }

    new_node->callback = callback;
    new_node->next = list->head;
    list->head = new_node;

    pthread_mutex_unlock(&list->mutex);
}

// 移除回调函数
void callback_list_unregister(CallbackList* list, DataCallback callback) {
    pthread_mutex_lock(&list->mutex);

    CallbackNode* current = list->head;
    CallbackNode* prev = NULL;

    while (current != NULL) {
        if (current->callback == callback) {
            if (prev == NULL) {
                list->head = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            break;
        }
        prev = current;
        current = current->next;
    }

    pthread_mutex_unlock(&list->mutex);
}

// 分发数据到所有回调函数
void callback_list_dispatch(CallbackList* list, void* data, int data_len) {
    pthread_mutex_lock(&list->mutex);

    CallbackNode* current = list->head;
    while (current != NULL) {
        current->callback(data, data_len);
        current = current->next;
    }

    pthread_mutex_unlock(&list->mutex);
}

// 销毁回调函数列表
void callback_list_destroy(CallbackList* list) {
    pthread_mutex_lock(&list->mutex);

    CallbackNode* current = list->head;
    while (current != NULL) {
        CallbackNode* next = current->next;
        free(current);
        current = next;
    }
    list->head = NULL;

    pthread_mutex_unlock(&list->mutex);
    pthread_mutex_destroy(&list->mutex);
}
