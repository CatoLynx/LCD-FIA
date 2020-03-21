#include <stdint.h>

void delay_us(uint16_t us);
double mapRange(double input, double inMin, double inMax, double outMin, double outMax);
double limitRange(double input, double min, double max);
void averageADCValues(uint32_t* in, uint32_t* out, uint8_t numChannels, uint32_t countPerChannel);