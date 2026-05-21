#ifndef POSIX_SHM_MANAGER_H
#define POSIX_SHM_MANAGER_H

#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

// TTLV缓存数据结构
typedef struct {
    int32_t action;        // ID1: action枚举值  
    int32_t times;         // ID2: 执行时间
    bool valid;            // 数据是否有效
} TtlvCacheData_t;

// 共享数据结构
typedef struct {
    bool data_ready;        // 数据就绪标志
    TtlvCacheData_t ttlv_data;  // TTLV缓存数据
    char data[1024];        // 额外数据缓冲区
} SharedData;

// 共享内存管理器类
typedef struct {
    int shm_fd;             // 共享内存文件描述符
    SharedData* data;       // 共享数据指针
    sem_t* semaphore_websocket;       // websocket命名信号量指针
    sem_t* semaphore_ros2;  // ros2信号量指针
} PosixShmManager;

// 函数声明
bool posix_shm_manager_init(PosixShmManager* manager, const char* shm_name, bool create_new);
void posix_shm_manager_cleanup(PosixShmManager* manager);
SharedData* posix_shm_manager_get_data(PosixShmManager* manager);

// 数据操作函数
bool posix_shm_write_control_data(PosixShmManager* manager, int32_t action, int32_t times, bool valid);
bool posix_shm_read_control_data(PosixShmManager* manager, int32_t* action, int32_t* times, bool* valid);
bool posix_shm_wait_for_data(PosixShmManager* manager, int32_t* action, int32_t* times, bool* valid);

// 信号量操作
bool posix_shm_signal_data_ready(PosixShmManager* manager);
bool posix_shm_wait_for_signal(PosixShmManager* manager);

// 工具函数
const char* posix_shm_get_error_string(void);
bool posix_shm_cleanup_resources(const char* shm_name);

#endif // POSIX_SHM_MANAGER_H
