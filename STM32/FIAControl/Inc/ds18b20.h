#pragma once

/*
  ds18b20.h / ds18b20.c - Library for the DS18B20 OneWire temperature sensor
*/

#include <stdint.h>
#include "stm32f4xx_hal.h"

#define DS18B20_ERROR 9001.0

uint8_t DS18B20_Start(GPIO_TypeDef* port, uint16_t pin);
void DS18B20_Write(GPIO_TypeDef* port, uint16_t pin, uint8_t data);
uint8_t DS18B20_Read(GPIO_TypeDef* port, uint16_t pin);
void DS18B20_ConvertTemperature(GPIO_TypeDef* port, uint16_t pin);
double DS18B20_ReadTemperature(GPIO_TypeDef* port, uint16_t pin);