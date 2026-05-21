#ifndef __AI_LOCAL_ASR_H__
#define __AI_LOCAL_ASR_H__
#include "ai_common_ringbuffer.h"
#include "ai_common_define.h"

typedef enum{
    CAR_ACTION_START,
	CAR_ADVANCE=2001,
    CAR_BACK,
    CAR_SHIFT_LEFT,
    CAR_SHIFT_RIGHT,
    CAR_TURN_LEFT,
    CAR_TURN_RIGHT,
    CAR_STOP_1,
    CAR_STOP_2,
    CAR_COME_OVER,
    CAR_FOLLOW_ME,
    CAR_EXIT=2021,
    CAR_ACTION_END,
}car_action_command;



int Intelligent_Asr_Init(module_desc_t * self_ops);

void run_asr_deal();
void stop_asr_deal();

#endif