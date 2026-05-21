#include "ai_common_ringbuffer.h"

RingBuffer* ringbuffer_init(size_t size) {
    RingBuffer *rb = (RingBuffer*)malloc(sizeof(RingBuffer));
    if (!rb) return NULL;

    rb->buffer = (unsigned char*)malloc(size);
    if (!rb->buffer) {
        free(rb);
        return NULL;
    }

    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;

    pthread_mutex_init(&rb->mutex, NULL);
    pthread_cond_init(&rb->cond, NULL);

    return rb;
}

void ringbuffer_destroy(RingBuffer *rb) {
    if (!rb) return;

    free(rb->buffer);
    pthread_mutex_destroy(&rb->mutex);
    pthread_cond_destroy(&rb->cond);
    free(rb);
}

size_t ringbuffer_write(RingBuffer *rb, char *data, size_t len) {
    if (!rb || !data || len == 0) return 0;

    pthread_mutex_lock(&rb->mutex);

    size_t written = 0;  // 已写入的字节数

    while (written < len) {
        // 计算当前可用空间
        size_t free_space = rb->size - rb->count;

        // 如果缓冲区已满，移动 tail 覆盖旧数据
        if (free_space == 0) {
            // 移动 tail 释放空间（覆盖最旧的数据）
            size_t to_free = len - written;  // 需要释放的空间
            if (to_free > rb->size) {
                to_free = rb->size;  // 不超过缓冲区大小
            }

            // 移动 tail（模拟覆盖）
            if (rb->tail + to_free <= rb->size) {
                rb->tail += to_free;
            } else {
                size_t first_len = rb->size - rb->tail;
                rb->tail = to_free - first_len;
            }

            rb->count -= to_free;  // 减少计数
            // 不实际擦除数据，直接覆盖
        }

        // 计算本次可写入的长度
        size_t chunk_len = len - written;
        if (chunk_len > rb->size - rb->count) {
            chunk_len = rb->size - rb->count;
        }

        // 写入数据
        if (rb->head + chunk_len <= rb->size) {
            memcpy(&rb->buffer[rb->head], &data[written], chunk_len);
            rb->head += chunk_len;
        } else {
            size_t first_len = rb->size - rb->head;
            memcpy(&rb->buffer[rb->head], &data[written], first_len);
            memcpy(rb->buffer, &data[written + first_len], chunk_len - first_len);
            rb->head = chunk_len - first_len;
        }

        rb->count += chunk_len;
        written += chunk_len;
    }

    // 通知读线程有新数据
    pthread_cond_signal(&rb->cond);

    pthread_mutex_unlock(&rb->mutex);
    return written;  // 返回实际写入的字节数
}

size_t ringbuffer_read_wait(RingBuffer *rb, char *data, size_t len) {
    if (!rb || !data || len == 0) return 0;

    pthread_mutex_lock(&rb->mutex);

    // 等待直到有足够的数据
    while (rb->count < len) {
        pthread_cond_wait(&rb->cond, &rb->mutex);
    }

    // 读取数据
    if (rb->tail + len <= rb->size) {
        memcpy(data, &rb->buffer[rb->tail], len);
        rb->tail += len;
    } else {
        // 分两段读取
        size_t first_len = rb->size - rb->tail;
        memcpy(data, &rb->buffer[rb->tail], first_len);
        memcpy(&data[first_len], rb->buffer, len - first_len);
        rb->tail = len - first_len;
    }

    rb->count -= len;

    pthread_mutex_unlock(&rb->mutex);

    return len;
}


size_t ringbuffer_read(RingBuffer *rb, char *data, size_t len) {
    if (!rb || !data || len == 0) return 0;

    pthread_mutex_lock(&rb->mutex);
    size_t available = rb->count;
    if (available <len) {
        pthread_mutex_unlock(&rb->mutex);
        return 0;  // 没有数据可读
    }

    // 只读取可用的数据
    size_t read_len = (len > available) ? available : len;

    if (rb->tail + read_len <= rb->size) {
        memcpy(data, &rb->buffer[rb->tail], read_len);
        rb->tail += read_len;
    } else {
        // 分两段读取
        size_t first_len = rb->size - rb->tail;
        memcpy(data, &rb->buffer[rb->tail], first_len);
        memcpy(&data[first_len], rb->buffer, read_len - first_len);
        rb->tail = read_len - first_len;
    }

    rb->count -= read_len;
    pthread_mutex_unlock(&rb->mutex);
    return read_len;
}


size_t ringbuffer_used(RingBuffer *rb) {
    if (!rb) return 0;
    pthread_mutex_lock(&rb->mutex);
    size_t used = rb->count;
    pthread_mutex_unlock(&rb->mutex);
    return used;
}

size_t ringbuffer_peek(RingBuffer *rb, char *data, size_t len) {
    if (!rb || !data || len == 0) return 0;

    pthread_mutex_lock(&rb->mutex);
    size_t available = rb->count;
    if (available < len) {
        pthread_mutex_unlock(&rb->mutex);
        return 0;  // 没有足够的数据可读
    }

    // 只读取可用的数据（不移动 tail 指针）
    size_t read_len = (len > available) ? available : len;
    size_t current_tail = rb->tail;  // 记录当前 tail 位置

    if (current_tail + read_len <= rb->size) {
        memcpy(data, &rb->buffer[current_tail], read_len);
    } else {
        // 分两段读取
        size_t first_len = rb->size - current_tail;
        memcpy(data, &rb->buffer[current_tail], first_len);
        memcpy(&data[first_len], rb->buffer, read_len - first_len);
    }

    pthread_mutex_unlock(&rb->mutex);
    return read_len;  // 返回实际读取的字节数
}


void ringbuffer_clear(RingBuffer *rb) {
    if (!rb) return;

    pthread_mutex_lock(&rb->mutex);
    // 重置 head、tail 和 count
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    // 可选：清零缓冲区内存（如果需要）
    memset(rb->buffer, 0, rb->size);
    pthread_mutex_unlock(&rb->mutex);
}

