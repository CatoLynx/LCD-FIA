#pragma once

/*
  util.h / util.c - Various small utility functions
*/

#include <stdint.h>

// Function prototypes
void delay_us(uint16_t us);
uint8_t reverseByte(uint8_t b);
double mapRange(double input, double inMin, double inMax, double outMin, double outMax);
double limitRange(double input, double min, double max);
void avgInterleaved(uint32_t* in, uint32_t* out, uint8_t numChannels, uint32_t countPerChannel);