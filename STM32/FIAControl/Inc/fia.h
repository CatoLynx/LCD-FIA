#pragma once

/*
  fia.h / fia.c - Library for the peripherals in the display

  This library contains the code to control the peripherals
  of the passenger information display.
  It handles reading sensors, controlling backlight brightness,
  switching fans etc.
*/

#include "adc.h"
#include "dac.h"
#include "globals.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"

// Peripheral definitions
#define LCD_SPI_ROW_1 hspi1
#define LCD_SPI_ROW_2 hspi2
#define LCD_SPI_ROW_3 hspi3
#define LCD_SPI_ROW_4 hspi4
#define BITMAP_DATA_SPI hspi5
#define LCD_FRAME_TIMER htim1
#define LCD_FRAME_TIMER_CHANNEL TIM_CHANNEL_1
#define DELAY_US_TIMER htim2
#define ADC_UPDATE_TIMER htim3
#define DS18B20_UPDATE_TIMER htim4
#define LCD_CONTRAST_DAC hdac
#define ENV_BRIGHTNESS_ADC hadc1
#define ADC_NUM_CHANNELS 3
#define PERIPHERALS_I2C hi2c1
#define CONTROL_UART huart10
#define DAC_LCD_CONTRAST_SIDE_A DAC1_CHANNEL_1
#define DAC_LCD_CONTRAST_SIDE_B DAC1_CHANNEL_2

// Peripheral timeout definitions
#define ADC_TIMEOUT 100
#define I2C_TIMEOUT 100

// I²C address definitions
#define I2C_ADDR_DAC_BRIGHTNESS_A (0x60 << 1)
#define I2C_ADDR_DAC_BRIGHTNESS_B (0x61 << 1)

// LCD related definitions
#define NUM_LCD_BUSES 4

// How many panels are daisy-chained.
// This assumes that first all top halves of each display
// are chained, then all bottom halves.
#define NUM_PANELS 5
#define NUM_HALF_PANELS (NUM_PANELS * 2)
#define PANEL_WIDTH 96
#define PANEL_HEIGHT 64
#define HALF_PANEL_NUM_BITMAP_BYTES 384
#define NUM_PANEL_ROWS 2

// Size of the bitmap data buffer (*2 because there are 2 LCD buses per side)
#define BITMAP_BUF_SIZE (HALF_PANEL_NUM_BITMAP_BYTES * NUM_HALF_PANELS * 2)

// Sensor related definitions
#define ADC_AVG_COUNT 200
#define TEMP_SENS_V25 0.76         // V
#define TEMP_SENS_AVG_SLOPE 0.0025 // V/°C

// Type definitions
typedef enum FIA_Side { SIDE_NONE = 0, SIDE_A = 1, SIDE_B = 2, SIDE_BOTH = 3 } FIA_Side_t;
typedef enum FIA_LCD_Bus { BUS_1 = 0, BUS_2 = 1, BUS_3 = 2, BUS_4 = 3 } FIA_LCD_Bus_t;
typedef enum FIA_Temp_Sensor { EXT_1 = 0, EXT_2 = 1, BOARD = 2, MCU = 3 } FIA_Temp_Sensor_t;

// Variables for brightness sensors
uint32_t adcRingBufferIndex;
uint32_t adcValues[ADC_NUM_CHANNELS];
uint32_t adcRingBuffer[ADC_NUM_CHANNELS * ADC_AVG_COUNT];
uint32_t adcAverages[ADC_NUM_CHANNELS];
uint16_t lcdContrast[2];
uint8_t updateLCDContrastFlag;
uint16_t envBrightness[2];
int16_t backlightBaseBrightness[2];
uint16_t backlightBrightness[2];
uint8_t updateBacklightBrightnessFlag;
uint8_t firstADCReadFlag;
uint8_t firstADCAverageFlag;

// Variables for receiving bitmap data from the high-level controller
uint8_t bitmapReceiveActive;
uint8_t bitmapBufferSideA[BITMAP_BUF_SIZE];
uint8_t bitmapBufferSideB[BITMAP_BUF_SIZE];

// Variables for temperature sensors
double tempSensorValues[4];

// Function prototypes
void FIA_Init(void);
void FIA_InitI2CDACs(void);
void FIA_MainLoop(void);
void FIA_ReceiveBitmapData(void);
void FIA_ReceiveBitmapData(void);
void FIA_AbortBitmapReceive(void);
void FIA_SetBacklightBrightness(FIA_Side_t side, uint16_t value);
void FIA_UpdateLCDContrast();
void FIA_SetLCDContrast(FIA_Side_t side, uint16_t value);
uint16_t FIA_GetLCDContrast(FIA_Side_t side);
void FIA_SetStatusLED(uint8_t number, uint8_t value);
uint16_t FIA_GetEnvBrightness(FIA_Side_t side);
void FIA_SetBacklightBaseBrightness(FIA_Side_t side, int16_t value);
int16_t FIA_GetBacklightBaseBrightness(FIA_Side_t side);
uint16_t FIA_GetBacklightBrightness(FIA_Side_t side);
uint16_t FIA_CalculateBacklightBrightness(FIA_Side_t side, uint16_t envBrt);
void FIA_UpdateADCValues(void);
void FIA_SetBacklight(uint8_t status);
uint8_t FIA_GetBacklight(void);
void FIA_SetHeaters(uint8_t level);
uint8_t FIA_GetHeaters(void);
void FIA_SetCirculationFans(uint8_t level);
uint8_t FIA_GetCirculationFans(void);
void FIA_SetHeatExchangerFan(uint8_t status);
uint8_t FIA_GetHeatExchangerFan(void);
void FIA_SetBacklightBallastFans(uint8_t status);
uint8_t FIA_GetBacklightBallastFans(void);
FIA_Side_t FIA_GetDoors(void);
void FIA_StartExtTempSensorConv(void);
void FIA_ReadTempSensors(void);
double FIA_GetTemperature(FIA_Temp_Sensor_t sensor);
double FIA_GetHumidity();