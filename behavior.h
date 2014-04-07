#ifndef _BEHAVIOR_H__
#define _BEHAVIOR_H__

#include "tlc5940.h"

extern uint8_t max_leds;
extern uint16_t max_intensity;

void bubblePop(channel_t channel_num);

#endif // _BEHAVIOR_H__