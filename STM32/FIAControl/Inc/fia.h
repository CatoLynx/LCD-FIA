#pragma once

#include "adc.h"
#include "dac.h"
#include "globals.h"
#include "i2c.h"
#include "tim.h"

#define LCD_SPI_ROW_1 hspi1
#define LCD_SPI_ROW_2 hspi2
#define LCD_SPI_ROW_3 hspi3
#define LCD_SPI_ROW_4 hspi4
#define BITMAP_DATA_SPI hspi5
#define LCD_LATCH_ROW_1 GPIOC, LCD1_LATCH_Pin
#define LCD_LATCH_ROW_2 GPIOE, LCD2_LATCH_Pin
#define LCD_LATCH_ROW_3 GPIOB, LCD3_LATCH_Pin
#define LCD_LATCH_ROW_4 GPIOE, LCD4_LATCH_Pin
#define LCD_FRAME_TIMER htim1
#define LCD_FRAME_TIMER_CHANNEL TIM_CHANNEL_1
#define DELAY_US_TIMER htim2
#define ADC_UPDATE_TIMER htim3
#define LCD_CONTRAST_DAC hdac
#define ENV_BRIGHTNESS_ADC hadc1
#define PERIPHERALS_I2C hi2c1
#define DAC_LCD_CONTRAST_SIDE_A DAC1_CHANNEL_1
#define DAC_LCD_CONTRAST_SIDE_B DAC1_CHANNEL_2

#define ADC_TIMEOUT 100
#define I2C_TIMEOUT 100

#define I2C_ADDR_DAC_BRIGHTNESS_A 0x60 << 1
#define I2C_ADDR_DAC_BRIGHTNESS_B 0x61 << 1

#define NUM_LCD_ROWS 4

#define ADC_NUM_CHANNELS 2
#define ADC_AVG_COUNT 200

typedef enum FIA_Side { SIDE_A = 0, SIDE_B = 1 } FIA_Side_t;
typedef enum FIA_LCD_Row { ROW_1 = 0, ROW_2 = 1, ROW_3 = 2, ROW_4 = 3 } FIA_LCD_Row_t;

uint32_t adcRingBufferIndex;
uint32_t adcValues[ADC_NUM_CHANNELS];
uint32_t adcRingBuffer[ADC_NUM_CHANNELS * ADC_AVG_COUNT];
uint32_t adcAverages[ADC_NUM_CHANNELS];
uint32_t envBrightness[2];
uint32_t baseBrightness[2];

uint8_t updateBacklightBrightnessFlag;
uint8_t firstADCReadFlag;
uint8_t firstADCAverageFlag;

void FIA_Init();
void FIA_InitI2CDACs();
void FIA_setBacklightBrightness(FIA_Side_t side, uint16_t value);
void FIA_setLCDContrast(FIA_Side_t side, uint16_t value);
void FIA_setStatusLED(uint8_t number, uint8_t value);
uint16_t FIA_getEnvBrightness(FIA_Side_t side);
void FIA_updateEnvBrightness();