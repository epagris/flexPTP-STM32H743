#ifndef TIMERSYNC_TIMERSYNC_H_
#define TIMERSYNC_TIMERSYNC_H_

#include "flexptp/timeutils.h"
#include <stm32h7xx_hal.h>

typedef struct {
    TIM_TypeDef * pTim;
    TimestampI ts[2];
    uint32_t period;
    int64_t err_ns[2];
    uint8_t skipCycles;
} ControllerState;

void timersync_init();
void timersync_update();
void timersync_stop();
void timersync_start();

#endif /* TIMERSYNC_TIMERSYNC_H_ */
