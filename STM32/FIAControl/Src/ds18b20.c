#include "ds18b20.h"
#include "stm32f4xx_hal.h"
#include "util.h"

uint8_t DS18B20_Start(GPIO_TypeDef* port, uint16_t pin) {
    uint8_t response = 0;
    setGPIOMode(port, pin, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW);
    HAL_GPIO_WritePin(port, pin, 0);
    delay_us(480);

    setGPIOMode(port, pin, GPIO_MODE_INPUT, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW);
    delay_us(80);

    if (!(HAL_GPIO_ReadPin(port, pin))) {
        response = 1;
    } else {
        response = 0;
    }
    delay_us(400);
    return response;
}

void DS18B20_Write(GPIO_TypeDef* port, uint16_t pin, uint8_t data) {
    setGPIOMode(port, pin, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW);

    for (int i = 0; i < 8; i++) {
        if ((data & (1 << i)) != 0) {
            setGPIOMode(port, pin, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW);
            HAL_GPIO_WritePin(port, pin, 0);
            delay_us(1);

            setGPIOMode(port, pin, GPIO_MODE_INPUT, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW);
            delay_us(60);
        } else {
            setGPIOMode(port, pin, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW);
            HAL_GPIO_WritePin(port, pin, 0);
            delay_us(60);

            setGPIOMode(port, pin, GPIO_MODE_INPUT, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW);
        }
    }
}

uint8_t DS18B20_Read(GPIO_TypeDef* port, uint16_t pin) {
    uint8_t value = 0;
    setGPIOMode(port, pin, GPIO_MODE_INPUT, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW);

    for (int i = 0; i < 8; i++) {
        setGPIOMode(port, pin, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW);

        HAL_GPIO_WritePin(port, pin, 0);
        delay_us(2);

        setGPIOMode(port, pin, GPIO_MODE_INPUT, GPIO_NOPULL, GPIO_SPEED_FREQ_LOW);
        if (HAL_GPIO_ReadPin(port, pin)) {
            value |= 1 << i;
        }
        delay_us(60);
    }
    return value;
}

void DS18B20_ConvertTemperature(GPIO_TypeDef* port, uint16_t pin) {
    uint8_t present = DS18B20_Start(port, pin);
    if (!present)
        return;
    delay_us(1000);
    DS18B20_Write(port, pin, 0xCC); // skip ROM
    DS18B20_Write(port, pin, 0x44); // convert t
}

double DS18B20_ReadTemperature(GPIO_TypeDef* port, uint16_t pin) {
    uint8_t present = DS18B20_Start(port, pin);
    if (!present)
        return DS18B20_ERROR;
    delay_us(1000);
    DS18B20_Write(port, pin, 0xCC); // skip ROM
    DS18B20_Write(port, pin, 0xBE); // Read Scratch-pad
    uint16_t tempLSB = DS18B20_Read(port, pin);
    uint16_t tempMSB = DS18B20_Read(port, pin);
    uint8_t sign = tempMSB & 0x80;
    int16_t temperature = (tempMSB << 8) | tempLSB;
    if (sign) {
        temperature = ((temperature ^ 0xFFFF) + 1) * -1;
    }
    return temperature / 16.0;
}