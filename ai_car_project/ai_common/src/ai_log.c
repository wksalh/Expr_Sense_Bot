#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#include "ai_log.h"


//常见的代码：
//0：重置所有属性
//1：高亮/加粗
//2：暗淡
//3：斜体
//4：下划线
//5：闪烁
//7：反转颜色（前景色和背景色互换）
//8：隐藏
//
//颜色代码（前景色）：
//30：黑色
//31：红色
//32：绿色
//33：黄色
//34：蓝色
//35：洋红
//36：青色
//37：白色
//
//背景颜色：
//40：黑色背景
//41：红色背景
//42：绿色背景
//43：黄色背景
//44：蓝色背景
//45：洋红背景
//46：青色背景
//47：白色背景

#define  RESET      "\033[0m\n"
#define  RED        "\033[31;40m"
#define  GRE        "\033[32;40m"
#define  YEL        "\033[33;40m"
#define  BLU        "\033[34;40m"
#define  YRE        "\033[35;40m"
#define  QSE        "\033[36;40m"
#define  WHI        "\033[37;40m"

#define  BUFFER_SIZE   1024
#define  LOG_FIX_STRING "smartcar_log"

int quec_ai_log_level  = LOG_AI_MAX;

int quec_ai_init(){
    // for extern fucntion or compatible
    return 0;
}

int quec_ai_log(int level,const char *fmt,...){

    if ( level > quec_ai_log_level ){
        return 0;
    }
    int ret = 0; 
    struct timeval tv;
    char time_str[30];
    char buffer[BUFFER_SIZE];
    char log_buffer[BUFFER_SIZE+100];
   // 获取当前时间（精确到微秒）
    gettimeofday(&tv, NULL);

    // 转换为本地时间
    struct tm *tm_info = localtime(&tv.tv_sec);

    // 格式化时间字符串（包含毫秒）
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    char time_ms_str[64];
    snprintf(time_ms_str, sizeof(time_ms_str), "[%s.%03ld]", time_str, tv.tv_usec / 1000);

    // 处理可变参数
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, BUFFER_SIZE, fmt, args);
    va_end(args);
    snprintf(log_buffer, sizeof(log_buffer), "[%s] %s", LOG_FIX_STRING, buffer);

    
    va_end(args);
    if ( level == LOG_AI_EMERG ){
        ret = printf(RED"[%s]:%s"RESET,time_ms_str,log_buffer);
    }
    if ( level == LOG_AI_ALERT ){
        ret = printf(RED"[%s]:%s"RESET,time_ms_str,log_buffer);
    }
    if ( level == LOG_AI_CRIT ){
        ret = printf(RED"[%s]:%s"RESET,time_ms_str,log_buffer);
    }
    if ( level == LOG_AI_ERR ){
        ret = printf(RED"[%s]:%s"RESET,time_ms_str,log_buffer);
    }
    if ( level == LOG_AI_WARNING ){
        ret = printf(YEL"[%s]:%s"RESET,time_ms_str,log_buffer);
    }
    if ( level == LOG_AI_NOTICE ){
        ret = printf(BLU"[%s]:%s"RESET,time_ms_str,log_buffer);
    }
    if ( level == LOG_AI_INFO ){
        ret = printf(QSE"[%s]:%s"RESET,time_ms_str,log_buffer);
    }
    if ( level == LOG_AI_DEBUG ){
        ret = printf(WHI"[%s]:%s"RESET,time_ms_str,log_buffer);
    }
    return ret;
}

int quec_ai_set_level(int level){
    quec_ai_log_level = level;
    return quec_ai_log_level;
}

int quec_ai_get_level(void){
    return quec_ai_log_level;
}
