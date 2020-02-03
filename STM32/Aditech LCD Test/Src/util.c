#include "util.h"
#include "globals.h"
#include "tim.h"
#include "stm32f1xx_hal.h"

void delay_us(uint16_t us) {
    __HAL_TIM_SET_COUNTER(&DELAY_US_TIMER, 0);
    while (__HAL_TIM_GET_COUNTER(&DELAY_US_TIMER) < us);
}