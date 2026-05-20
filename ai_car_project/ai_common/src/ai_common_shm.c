/*************************************************************************
 * @author: Leox Wee
 * @version: V1.0.0
 * @date:
 * @brief:
 * @hardware: 任何ANSI-C平台
 * @other:
 ***************************************************************************/

 #include <stdbool.h>
 #include <sys/mman.h>
 #include <semaphore.h>
 #include <fcntl.h>
 #include <unistd.h>
 #include <errno.h>
 #include <string.h>
 #include <signal.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <sys/stat.h>
 #include <stdint.h>
 #include <pthread.h>
 #include "ai_websocket.h"
 #include "ai_log.h"
#include "ai_common_shm.h"

 

  // 共享内存通信全局变量
 static PosixShmManager g_shm_manager = {0};
 
 
  // 清理IPC资源
 void cleanup_ipc_resources() 
 {
    if (g_shm_manager.data && g_shm_manager.data != MAP_FAILED)
    {
        munmap(g_shm_manager.data, sizeof(SharedData));
        g_shm_manager.data = NULL;
    }
 
    if (g_shm_manager.shm_fd != -1)
    {
        close(g_shm_manager.shm_fd);
        g_shm_manager.shm_fd = -1;
    }
 
    if (g_shm_manager.semaphore_ros2 && g_shm_manager.semaphore_ros2 != SEM_FAILED)
    {
        sem_close(g_shm_manager.semaphore_ros2);
        g_shm_manager.semaphore_ros2 = NULL;
    }
    if (g_shm_manager.semaphore_websocket && g_shm_manager.semaphore_websocket != SEM_FAILED)
    {
        sem_close(g_shm_manager.semaphore_websocket);
        g_shm_manager.semaphore_websocket = NULL;
    }
 
     quec_ai_log(LOG_AI_INFO,"IPC资源清理完成\n");
 }


  // 发送TTLV数据到ROS2进程
 int send_ttlv_data_to_ros2(int32_t action, int32_t times, bool valid) 
 {
     
     if (!g_shm_manager.data || !g_shm_manager.semaphore_ros2)
     {
         quec_ai_log(LOG_AI_INFO,"错误: 共享内存IPC未初始化\n");
         return -1;
     }
 
     // 设置TTLV数据
     g_shm_manager.data->ttlv_data.action = action;
     g_shm_manager.data->ttlv_data.times = times;
     g_shm_manager.data->ttlv_data.valid = valid;
 
     // 设置数据就绪标志
     g_shm_manager.data->data_ready = true;
     
     #if 0
     // 检查发送前的信号量值
     int sem_value_before;
     if (sem_getvalue(g_shm_manager.semaphore_ros2, &sem_value_before) == 0)
     {
         quec_ai_log(LOG_AI_INFO,"DEBUG: 发送前信号量值: %d\n", sem_value_before);
     }
     #endif
     
    if (sem_post(g_shm_manager.semaphore_ros2) == -1)
    {
        quec_ai_log(LOG_AI_INFO,"错误: 无法发送信号量: %s\n", strerror(errno));
        return -2;
    }
     
     quec_ai_log(LOG_AI_INFO,"DEBUG: 信号量发送成功\n");
     
     #if 0
     // 检查发送后的信号量值
     int sem_value_after;
    if (sem_getvalue(g_shm_manager.semaphore_ros2, &sem_value_after) == 0)
    {
        quec_ai_log(LOG_AI_INFO,"DEBUG: 发送后信号量值: %d\n", sem_value_after);
    }
    else
    {
        quec_ai_log(LOG_AI_INFO,"DEBUG: 无法获取信号量值: %s\n", strerror(errno));
    }
     #endif
     
     // 确保数据写入完成后再发送信号量
     // 这里不需要额外的同步，因为sem_post()已经确保了内存可见性
 
     quec_ai_log(LOG_AI_INFO,"发送TTLV数据到ROS2: action=%d, times=%dms, valid=%s\n",
            action, times, valid ? "true" : "false");
     return 0;
 }

char *take_screenshot()
{
    quec_ai_log(LOG_AI_INFO,"发送截图请求 (action=12)...\n");
    if (send_ttlv_data_to_ros2(12, 0, true) != 0)
    {
        quec_ai_log(LOG_AI_INFO,"发送截图请求失败\n");
        return NULL;
    }

    quec_ai_log(LOG_AI_INFO,"等待ROS2端处理截图...\n");
    quec_ai_log(LOG_AI_INFO,"等待标志文件出现...\n");

    // 使用更短的轮询间隔（100ms）以提高响应速度
    int retry_count = 0;
    int max_retries = 10;  // 20次 * 100ms = 1秒
    char *screenshot_path = "/tmp/qth_shot_latest.jpg";
    char *result = NULL;
    int flag_found = 0;
    
    // 先立即检查一次，避免不必要的延迟
    int flag_fd = open("/tmp/screenshot_done.flag", O_RDONLY);
    if (flag_fd >= 0)
    {
        close(flag_fd);
        quec_ai_log(LOG_AI_INFO,"收到截图完成标志文件（立即检测到）\n");
        flag_found = 1;
    }
    else
    {

        while (retry_count < max_retries)
        {
            usleep(100000);  // 100毫秒
            retry_count++;
            flag_fd = open("/tmp/screenshot_done.flag", O_RDONLY);
            if (flag_fd >= 0)
            {
                close(flag_fd);
                flag_found = 1;
                break;
            }

            if (retry_count % 10 == 0)
            {
                quec_ai_log(LOG_AI_INFO,"等待中... (%d/%d) - 检查文件: /tmp/screenshot_done.flag\n",
                       retry_count, max_retries);
            }
        }
    }
    
    if (!flag_found)
    {
        quec_ai_log(LOG_AI_INFO,"等待截图完成超时\n");
        return NULL;
    }

    // 检查截图文件是否生成
    int screenshot_fd = open(screenshot_path, O_RDONLY);
    if (screenshot_fd >= 0)
    {
        close(screenshot_fd);
        quec_ai_log(LOG_AI_INFO,"截图文件已生成: %s\n", screenshot_path);

        // 复制路径字符串
        result = strdup(screenshot_path);
        if (result == NULL) {
            quec_ai_log(LOG_AI_INFO,"内存分配失败\n");
            return NULL;
        }

        quec_ai_log(LOG_AI_INFO,"截图功能测试成功！\n");
    }
    else
    {
        quec_ai_log(LOG_AI_INFO,"截图文件未找到: %s\n", screenshot_path);
        quec_ai_log(LOG_AI_INFO,"截图功能测试失败\n");
        return NULL;
    }
   
    // 清理标志文件
    quec_ai_log(LOG_AI_INFO,"开始清理标志文件...\n");
    if (unlink("/tmp/screenshot_done.flag") == 0 || errno == ENOENT)
    {
        quec_ai_log(LOG_AI_INFO,"标志文件已清理\n");
    }
    else
    {
        quec_ai_log(LOG_AI_INFO,"警告: 清理标志文件失败: %s\n", strerror(errno));
    }
   
    return result;
}

// 初始化共享内存和信号量
 int init_shared_memory_ipc(const char* shm_name) 
 {
     // 初始化管理器结构
     memset(&g_shm_manager, 0, sizeof(PosixShmManager));
     
     // 创建信号量名称
     char sem_name_ros2[256];
     char sem_name_websocket[256];
     snprintf(sem_name_ros2, sizeof(sem_name_ros2), "%s_sem_ros2", shm_name);
     snprintf(sem_name_websocket, sizeof(sem_name_websocket), "%s_sem_websocket", shm_name);
 
     // 打开或创建共享内存
     g_shm_manager.shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (g_shm_manager.shm_fd == -1)
    {
        quec_ai_log(LOG_AI_INFO,"错误: 无法创建共享内存: %s\n", strerror(errno));
        return -1;
    }
 
     // 检查共享内存是否已存在，如果不存在才设置大小
     struct stat st;
    if (fstat(g_shm_manager.shm_fd, &st) == -1)
    {
        quec_ai_log(LOG_AI_INFO,"错误: 无法获取共享内存状态: %s\n", strerror(errno));
        close(g_shm_manager.shm_fd);
        return -1;
    }
     
    if (st.st_size == 0)
    {
        // 共享内存不存在，设置大小
        if (ftruncate(g_shm_manager.shm_fd, sizeof(SharedData)) == -1)
        {
            quec_ai_log(LOG_AI_INFO,"错误: 无法设置共享内存大小: %s\n", strerror(errno));
            close(g_shm_manager.shm_fd);
            return -1;
        }
        quec_ai_log(LOG_AI_INFO,"创建新的共享内存，%zu,%lld\n",sizeof(SharedData),(long long)st.st_size);
    }
    else
    {
        quec_ai_log(LOG_AI_INFO,"使用已存在的共享内存\n");
    }
 
     // 映射共享内存
     g_shm_manager.data = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, g_shm_manager.shm_fd, 0);
    if (g_shm_manager.data == MAP_FAILED)
    {
        quec_ai_log(LOG_AI_INFO,"错误: 无法映射共享内存: %s\n", strerror(errno));
        close(g_shm_manager.shm_fd);
        return -1;
    }
 
     // 创建或打开命名信号量
     g_shm_manager.semaphore_ros2 = sem_open(sem_name_ros2, O_CREAT, 0666, 0);
    if (g_shm_manager.semaphore_ros2 == SEM_FAILED)
    {
        quec_ai_log(LOG_AI_INFO,"错误: 无法创建信号量 ros2: %s\n", strerror(errno));
        munmap(g_shm_manager.data, sizeof(SharedData));
        close(g_shm_manager.shm_fd);
        return -1;
    }

    g_shm_manager.semaphore_websocket = sem_open(sem_name_websocket, O_CREAT, 0666, 0);
    if (g_shm_manager.semaphore_websocket == SEM_FAILED)
    {
        quec_ai_log(LOG_AI_INFO,"错误: 无法创建信号量websocket: %s\n", strerror(errno));
        munmap(g_shm_manager.data, sizeof(SharedData));
        close(g_shm_manager.shm_fd);
        return -1;
    }
 
     // 只在创建新共享内存时初始化数据
    if (st.st_size == 0)
    {
        g_shm_manager.data->data_ready = false;
        memset(&g_shm_manager.data->ttlv_data, 0, sizeof(TtlvCacheData_t));
        quec_ai_log(LOG_AI_INFO,"初始化共享内存数据\n");
    }
    else
    {
        quec_ai_log(LOG_AI_INFO,"保持现有共享内存数据\n");
    }
 
     quec_ai_log(LOG_AI_INFO,"共享内存IPC初始化成功: %s\n", shm_name);
     return 0;
 }