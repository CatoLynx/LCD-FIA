#include "util.h"
#include "fia.h"
#include "stm32f4xx_hal.h"
#include "tim.h"

void delay_us(uint16_t us) {
    __HAL_TIM_SET_COUNTER(&DELAY_US_TIMER, 0);
    while (__HAL_TIM_GET_COUNTER(&DELAY_US_TIMER) < us);
}

double mapRange(double input, double inMin, double inMax, double outMin, double outMax) {
    return (input - inMin) / (inMax - inMin) * (outMax - outMin) + outMin;
}

double limitRange(double input, double min, double max) {
    if(input < min) return min;
    if(input > max) return max;
    return input;
}

void averageADCValues(uint32_t* in, uint32_t* out, uint8_t numChannels, uint32_t countPerChannel) {
    for(uint8_t c = 0; c < numChannels; c++) {
        uint32_t i = c;
        uint64_t avg = 0;
        for(uint32_t n = 0; n < countPerChannel; n++) {
            avg += in[i];
            i += numChannels;
        }
        avg /= countPerChannel;
        out[c] = avg;
    }
}