#include "util.h"
#include "fia.h"
#include "stm32f4xx_hal.h"
#include "tim.h"

void delay_us(uint16_t us) {
    // Blocking microsecond delay using a hardware timer
    __HAL_TIM_SET_COUNTER(&DELAY_US_TIMER, 0);
    while (__HAL_TIM_GET_COUNTER(&DELAY_US_TIMER) < us)
        ;
}

uint8_t reverseByte(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

double mapRange(double input, double inMin, double inMax, double outMin, double outMax) {
    // Map an arbitrary input range to an arbitrary output range
    return (input - inMin) / (inMax - inMin) * (outMax - outMin) + outMin;
}

double limitRange(double input, double min, double max) {
    // Make sure values are limited to a given range
    if (input < min)
        return min;
    if (input > max)
        return max;
    return input;
}

void avgInterleaved(uint32_t* in, uint32_t* out, uint8_t numChannels, uint32_t countPerChannel) {
    // Take an array of interleaved value groups, average each one of them
    // and write the averages to a second array, given the number of groups
    // and the number of values to average per group
    for (uint8_t c = 0; c < numChannels; c++) {
        uint32_t i = c;
        uint64_t avg = 0;
        for (uint32_t n = 0; n < countPerChannel; n++) {
            avg += in[i];
            i += numChannels;
        }
        avg /= countPerChannel;
        out[c] = avg;
    }
}

void setGPIOMode(GPIO_TypeDef* port, uint16_t pin, uint16_t mode, uint16_t pull, uint16_t speed) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = pin;
    GPIO_InitStruct.Mode = mode;
    GPIO_InitStruct.Pull = pull;
    GPIO_InitStruct.Speed = speed;
    HAL_GPIO_Init(port, &GPIO_InitStruct);
}

uint16_t roundUp(uint16_t numToRound, uint16_t multiple) {
    // Round a number up to the nearest multiple of x
    if (multiple == 0)
        return numToRound;

    uint16_t remainder = numToRound % multiple;
    if (remainder == 0)
        return numToRound;

    return numToRound + multiple - remainder;
}