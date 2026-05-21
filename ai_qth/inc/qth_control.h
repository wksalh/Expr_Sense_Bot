#ifndef __QTH_CONTROL_H__
#define __QTH_CONTROL_H__

#include <stdbool.h>

typedef enum
{
    /*小车运动事件类型*/


    /*音频播放事件*/
    QTH_ONLINE_EXIT=101,
    QTH_MAIX_MIC_CAR,
    QTH_CONNECT_WIFI,
    QTH_CONNECT_WIFI_AUDIO,

    /*网络连接事件*/
    QTH_RESGTER_NETWORK_SUCCECC = 201,
    QTH_RESGTER_NETWORK_ERROR,
    // 可以根据需要添加更多事件类型
} Qth_eventType_e;


typedef void (*QthDataCallback)(int action, int times, bool valid);

int qth_init(QthDataCallback callback);
int Qth_devStart(void);
int Qth_devStop(void);
int Qth_devRemove(void);

#endif