#include "posix_shm_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

// 全局错误字符串
static char error_string[256] = {0};
static int error_string_initialized = 0;

// 全局析构函数
__attribute__((destructor))
static void cleanup_global_resources(void) 
{
    // 清理全局资源
    error_string_initialized = 0;
    memset(error_string, 0, sizeof(error_string));
}

// 添加一个安全的错误字符串获取函数
const char* posix_shm_get_error_string_safe(void)
{
    if (!error_string_initialized) 
    {
        return "Error string not initialized";
    }
    return error_string;
}

// 获取错误字符串
const char* posix_shm_get_error_string(void) 
{
    if (!error_string_initialized)
    {
        return "Error string not initialized";
    }
    return error_string;
}

// 设置错误字符串
static void set_error(const char* format, ...) 
{
    va_list args;
    va_start(args, format);
    vsnprintf(error_string, sizeof(error_string), format, args);
    va_end(args);
    error_string_initialized = 1;
}

// 初始化共享内存管理器
bool posix_shm_manager_init(PosixShmManager* manager, const char* shm_name, bool create_new) 
{
    if (!manager || !shm_name) 
    {
        set_error("Invalid parameters");
        return false;
    }
    
    // 初始化管理器结构
    memset(manager, 0, sizeof(PosixShmManager));
    
    // 创建信号量名称（使用_sem_ros2后缀以匹配qth端）
    char sem_name[256];
    snprintf(sem_name, sizeof(sem_name), "%s_sem_ros2", shm_name);
    
    if (create_new) 
    {
        // 创建共享内存
        manager->shm_fd = shm_open(shm_name, O_CREAT | O_RDWR | O_EXCL, 0666);
        if (manager->shm_fd == -1) 
        {
            if (errno == EEXIST) 
            {
                // 如果已存在，先删除再创建
                shm_unlink(shm_name);
                manager->shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
            }
            if (manager->shm_fd == -1) 
            {
                set_error("Failed to create shared memory: %s", strerror(errno));
                return false;
            }
        }
        
        // 设置共享内存大小
        if (ftruncate(manager->shm_fd, sizeof(SharedData)) == -1) 
        {
            set_error("Failed to set shared memory size: %s", strerror(errno));
            close(manager->shm_fd);
            return false;
        }
    } 
    else 
    {
        // 打开已存在的共享内存
        manager->shm_fd = shm_open(shm_name, O_RDWR, 0666);
        if (manager->shm_fd == -1) {
            set_error("Failed to open shared memory: %s", strerror(errno));
            return false;
        }
    }
    
    // 内存映射
    manager->data = (SharedData*)mmap(NULL, sizeof(SharedData), 
                                     PROT_READ | PROT_WRITE, MAP_SHARED, 
                                     manager->shm_fd, 0);
    if (manager->data == MAP_FAILED) 
    {
        set_error("Failed to map shared memory: %s", strerror(errno));
        close(manager->shm_fd);
        return false;
    }
    
    // 尝试打开已存在的命名信号量，如果不存在则创建
    manager->semaphore_ros2 = sem_open(sem_name, O_CREAT, 0666, 0);
    if (manager->semaphore_ros2 == SEM_FAILED) 
    {
        set_error("Failed to open/create semaphore: %s", strerror(errno));
        munmap(manager->data, sizeof(SharedData));
        close(manager->shm_fd);
        return false;
    }
    
    if (create_new) 
    {
        // 初始化数据
        manager->data->data_ready = false;
        manager->data->ttlv_data.action = 0;
        manager->data->ttlv_data.times = 0;
        manager->data->ttlv_data.valid = false;
        memset(manager->data->data, 0, sizeof(manager->data->data));
    }
    
    return true;
}

// 清理共享内存管理器
void posix_shm_manager_cleanup(PosixShmManager* manager) 
{
    if (!manager) return;
    
    // 安全地取消内存映射
    if (manager->data && manager->data != MAP_FAILED && manager->data != NULL) 
    {
        munmap(manager->data, sizeof(SharedData));
        manager->data = NULL;
    }
    
    // 安全地关闭命名信号量
    if (manager->semaphore_ros2 && manager->semaphore_ros2 != SEM_FAILED && manager->semaphore_ros2 != NULL)
    {
        sem_close(manager->semaphore_ros2);
        manager->semaphore_ros2 = NULL;
    }
    
    // 安全地关闭文件描述符
    if (manager->shm_fd != -1) 
    {
        close(manager->shm_fd);
        manager->shm_fd = -1;
    }
}

// 获取共享数据指针
SharedData* posix_shm_manager_get_data(PosixShmManager* manager) 
{
    if (!manager || !manager->data) 
    {
        return NULL;
    }
    return manager->data;
}

// 写入控制数据
bool posix_shm_write_control_data(PosixShmManager* manager, int32_t action, int32_t times, bool valid) 
{
    if (!manager || !manager->data) 
    {
        set_error("Manager not initialized");
        return false;
    }
    
    // 写入控制数据
    manager->data->ttlv_data.action = action;
    manager->data->ttlv_data.times = times;
    manager->data->ttlv_data.valid = valid;
    manager->data->data_ready = true;
    
    // 释放信号量，通知读取进程
    if (sem_post(manager->semaphore_ros2) == -1) 
    {
        set_error("Failed to post semaphore: %s", strerror(errno));
        return false;
    }
    
    return true;
}

// 读取控制数据
bool posix_shm_read_control_data(PosixShmManager* manager, int32_t* action, int32_t* times, bool* valid) 
{
    if (!manager || !manager->data || !action || !times || !valid)
    {
        set_error("Invalid parameters");
        return false;
    }
    
    if (!manager->data->data_ready) 
    {
        return false;
    }
    
    // 读取数据
    *action = manager->data->ttlv_data.action;
    *times = manager->data->ttlv_data.times;
    *valid = manager->data->ttlv_data.valid;
    
    // 重置数据就绪标志
    manager->data->data_ready = false;
    
    return true;
}

// 等待数据并读取（阻塞等待）
bool posix_shm_wait_for_data(PosixShmManager* manager, int32_t* action, int32_t* times, bool* valid) 
{
    if (!manager || !manager->data || !action || !times || !valid) 
    {
        set_error("Invalid parameters");
        return false;
    }
    
    // 等待信号量
    if (sem_wait(manager->semaphore_ros2) == -1) 
    {
        set_error("Failed to wait for semaphore: %s", strerror(errno));
        return false;
    }
    
    // 检查数据是否就绪
    if (!manager->data->data_ready) 
    {
        return false;
    }
    
    // 读取数据
    *action = manager->data->ttlv_data.action;
    *times = manager->data->ttlv_data.times;
    *valid = manager->data->ttlv_data.valid;
    
    // 重置数据就绪标志
    manager->data->data_ready = false;
    
    return true;
}

// 发送数据就绪信号
bool posix_shm_signal_data_ready(PosixShmManager* manager) 
{
    if (!manager || !manager->semaphore_ros2) 
    {
        set_error("Manager not initialized");
        return false;
    }
    
    if (sem_post(manager->semaphore_ros2) == -1) 
    {
        set_error("Failed to post semaphore: %s", strerror(errno));
        return false;
    }
    
    return true;
}

// 非阻塞检查信号量
bool posix_shm_wait_for_signal(PosixShmManager* manager) 
{
    if (!manager || !manager->semaphore_ros2) 
    {
        set_error("Manager not initialized");
        return false;
    }
    
    // 检查信号量指针是否有效
    if (manager->semaphore_ros2 == NULL || manager->semaphore_ros2 == SEM_FAILED) 
    {
        return false;
    }
    
    int result = sem_trywait(manager->semaphore_ros2);
    
    if (result == -1) 
    {
        if (errno == EAGAIN) 
        {
            // 没有信号量可用，这是正常情况
            return false;
        }
        set_error("Failed to try wait for semaphore: %s", strerror(errno));
        return false;
    }
    
    return true;
}

// 清理共享内存资源
bool posix_shm_cleanup_resources(const char* shm_name) 
{
    if (!shm_name) 
    {
        set_error("Invalid shared memory name");
        return false;
    }
    
    char sem_name[256];
    snprintf(sem_name, sizeof(sem_name), "%s_sem_ros2", shm_name);
    
    // 删除共享内存
    if (shm_unlink(shm_name) == -1 && errno != ENOENT) 
    {
        set_error("Failed to unlink shared memory: %s", strerror(errno));
        return false;
    }
    
    // 删除信号量
    if (sem_unlink(sem_name) == -1 && errno != ENOENT) 
    {
        set_error("Failed to unlink semaphore: %s", strerror(errno));
        return false;
    }
    
    return true;
}
