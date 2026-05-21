#ifndef __QUEC_AI_LOG_H__
#define __QUEC_AI_LOG_H__

#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_AI_CLOSE,
    LOG_AI_EMERG,
    LOG_AI_ALERT,
    LOG_AI_CRIT,
    LOG_AI_ERR,
    LOG_AI_WARNING,
    LOG_AI_NOTICE,
    LOG_AI_DEBUG,
    LOG_AI_INFO,
    LOG_AI_MAX,
}Log_level;

int quec_ai_init();

int quec_ai_log(int level,const char *fmt,...);

int quec_ai_set_level(int level);

int quec_ai_get_level(void);

#ifdef __cplusplus
}
#endif

#endif //__QUEC_AI_LOG_H__

