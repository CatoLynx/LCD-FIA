#pragma once

/*
  util.h / util.c - Various small utility functions
*/

#include <stdint.h>
#include "stm32f4xx_hal.h"

// Function prototypes
void delay_us(uint16_t us);
uint8_t reverseByte(uint8_t b);
double mapRange(double input, double inMin, double inMax, double outMin, double outMax);
double limitRange(double input, double min, double max);
void avgInterleaved(uint32_t* in, uint32_t* out, uint8_t numChannels, uint32_t countPerChannel);
void setGPIOMode(GPIO_TypeDef* port, uint16_t pin, uint16_t mode, uint16_t pull, uint16_t speed);
uint16_t roundUp(uint16_t numToRound, uint16_t multiple);