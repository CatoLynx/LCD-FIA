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

#if defined(DISPLAY_FORMAT_5x2)
#    include "splash_5x2.h"
#    define SPLASH_ENABLED
#elif defined(DISPLAY_FORMAT_4x2)
#    include "splash_4x2.h"
#    define SPLASH_ENABLED
#endif

// Peripheral definitions
#define LCD_SPI_ROW_1 hspi1
#define LCD_SPI_ROW_2 hspi2
#define LCD_SPI_ROW_3 hspi3
#define LCD_SPI_ROW_4 hspi4
#define BITMAP_DATA_SPI hspi5
#define LCD_FRAME_TIMER htim1 // f: See LCD_FR_INTERVAL comment
#define LCD_FRAME_TIMER_CHANNEL TIM_CHANNEL_1
#define DELAY_US_TIMER htim2       // No fixed frequency, used for delay
#define ADC_UPDATE_TIMER htim3     // f = 16 Hz
#define DS18B20_UPDATE_TIMER htim4 // f = 0.2 Hz
#define SCROLL_UPDATE_TIMER htim5  // f = 100 Hz
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
#define LCD_FR_INTERVAL 1400 // Must be even. Formula: i = F_CPU/(200*f) or f = F_CPU/(200*i)

// How many panels are daisy-chained.
// This assumes that first all top halves of each display
// are chained, then all bottom halves.
#if defined(DISPLAY_FORMAT_5x2)
#define NUM_PANELS 5
#elif defined(DISPLAY_FORMAT_4x2)
#define NUM_PANELS 4
#endif

#define NUM_HALF_PANELS (NUM_PANELS * 2)
#define PANEL_WIDTH 96
#define PANEL_HEIGHT 64
#define HALF_PANEL_NUM_BITMAP_BYTES 384
#define NUM_PANEL_ROWS 2

// Size of the bitmap data buffer per side
#define BITMAP_BUF_SIZE (HALF_PANEL_NUM_BITMAP_BYTES * NUM_HALF_PANELS * NUM_PANEL_ROWS)

// Bitmap and scroll buffer related definitions
#define BITMAP_BUF_H_PX (PANEL_HEIGHT * NUM_PANEL_ROWS)
#define BITMAP_BUF_H_BYTES (BITMAP_BUF_H_PX / 8)
#define BITMAP_BUF_W_PX (PANEL_WIDTH * NUM_PANELS)
#define MAX_SCROLL_BUFFERS 20
#define SCROLL_BUFFER_ID_MASK 0x80
#define MASK_BUFFER_ID_MASK 0x40
#define DYN_BUFFER_ID_MASK 0x20
#define SCROLL_BUFFER_ERR_MASK 0x10
#define SCROLL_BUFFER_ERR_COUNT 1
#define SCROLL_BUFFER_ERR_SIZE 2

// Sensor related definitions
#define ADC_AVG_COUNT 200
#define TEMP_SENS_V25 0.76         // V
#define TEMP_SENS_AVG_SLOPE 0.0025 // V/°C

// Temperature regulation
#define BL_BALLAST_FANS_ON_TEMP 35
#define BL_BALLAST_FANS_OFF_TEMP 30
#define CIRCULATION_FANS_HALF_TEMP 40
#define CIRCULATION_FANS_FULL_TEMP 45
#define CIRCULATION_FANS_OFF_TEMP 35
#define CIRCULATION_FANS_BL_BALLAST_ON_TEMP 60
#define CIRCULATION_FANS_BL_BALLAST_OFF_TEMP 50
#define HEAT_EXCHANGER_FAN_ON_TEMP 40
#define HEAT_EXCHANGER_FAN_OFF_TEMP 35
#define HEATERS_HALF_TEMP 15
#define HEATERS_FULL_TEMP 10
#define HEATERS_OFF_TEMP 20
#define HEATERS_HALF_HUMIDITY 75
#define HEATERS_FULL_HUMIDITY 80
#define HEATERS_OFF_HUMIDITY 65
#define HEATERS_CUTOFF_TEMP 35

// Type definitions
typedef enum FIA_Side { SIDE_NONE = 0, SIDE_A = 1, SIDE_B = 2, SIDE_BOTH = 3 } FIA_Side_t;
typedef enum FIA_LCD_Bus { BUS_1 = 0, BUS_2 = 1, BUS_3 = 2, BUS_4 = 3 } FIA_LCD_Bus_t;
typedef enum FIA_Temp_Sensor { BL_BALL = 0, AIRFLOW = 1, BOARD = 2, MCU = 3 } FIA_Temp_Sensor_t;

typedef struct FIA_Scroll_Buffer {
    uint8_t occupied; // Whether this slot is in use
    FIA_Side_t side;  // Which side of the display it applies to
    uint16_t dispX;   // X coordinate of upper left corner of scroll window
    uint16_t dispY;   // Y coordinate of upper left corner of scroll window
    uint16_t dispW;   // Displayed width of the scroll window
    uint16_t dispH;   // Displayed height of the scroll window
    uint16_t intW;    // Internal width of the scroll buffer in pixels
    uint16_t intH;    // Internal height of the scroll buffer in pixels (rounded up to multiple of 8, used for bufSize
                      // calculation)
    size_t bufSize;   // Size of the bitmap data buffer in bytes
    uint8_t* buf;     // The bitmap data buffer
    uint16_t scrollOffsetX; // The current scroll viewport offsets
    uint16_t scrollOffsetY;
    uint16_t scrollSpeedX; // Scroll interval (= 100 Hz / speed)
    uint16_t scrollSpeedY;
    int16_t scrollStepX; // Scroll by (step) pixels each interval
    int16_t scrollStepY;
    uint16_t scrollTickCntX; // Internal counters for the number of scroll timer ticks
    uint16_t scrollTickCntY;
} FIA_Scroll_Buffer_t;

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
uint8_t FIA_staticBufferSideA[BITMAP_BUF_SIZE];
uint8_t FIA_staticBufferSideB[BITMAP_BUF_SIZE];
uint8_t FIA_dynamicBufferSideA[BITMAP_BUF_SIZE];
uint8_t FIA_dynamicBufferSideB[BITMAP_BUF_SIZE];
uint8_t FIA_maskBufferSideA[BITMAP_BUF_SIZE];
uint8_t FIA_maskBufferSideB[BITMAP_BUF_SIZE];
uint8_t FIA_displayBufferSideA[BITMAP_BUF_SIZE];
uint8_t FIA_displayBufferSideB[BITMAP_BUF_SIZE];
uint8_t FIA_bitmapRxActive;
uint8_t* FIA_bitmapRxBuf;
uint8_t FIA_bitmapRxBufID;
uint8_t FIA_bitmapRxBoth;
uint16_t FIA_bitmapRxLen;
uint8_t FIA_maskEnabled;

// Variables for scroll buffers
FIA_Scroll_Buffer_t FIA_scrollBuffers[MAX_SCROLL_BUFFERS];
uint8_t FIA_scrollBufferCount;
int8_t FIA_nextFreeScrollBufferIndex;

// Variables for temperature sensors
double FIA_tempSensorValues[4];
uint8_t FIA_circulationFansOverrideBLBallast;
uint8_t FIA_circulationFansOverrideHeatersTemp;
uint8_t FIA_circulationFansOverrideHeatersHumidity;

// Function prototypes
void FIA_Init(void);
void FIA_InitI2CDACs(void);
void FIA_MainLoop(void);
void FIA_UpdateScrollPositions(void);
void FIA_ScrollBufferRelative(FIA_Scroll_Buffer_t* buf, int16_t xStep, int16_t yStep);
void FIA_RenderScrollBuffer(FIA_Scroll_Buffer_t buf);
void FIA_RenderScrollBuffers(void);
void FIA_UpdateDisplayBuffers(void);
void FIA_UpdateDisplay(FIA_LCD_Bus_t bus);
void FIA_StartBitmapReceive(void);
void FIA_AbortBitmapReceive(void);
void FIA_SetBacklightBrightness(FIA_Side_t side, uint16_t value);
void FIA_UpdateLCDContrast(void);
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
double FIA_GetHumidity(void);
uint8_t FIA_CreateScrollBuffer(FIA_Side_t side, uint16_t dispX, uint16_t dispY, uint16_t dispW, uint16_t dispH,
                               uint16_t intW, uint16_t intH, uint16_t scOffX, uint16_t scOffY, uint16_t scSpX,
                               uint16_t scSpY, int16_t scStX, int16_t scStY);
uint8_t FIA_UpdateScrollBuffer(uint8_t id, FIA_Side_t side, uint16_t dispX, uint16_t dispY, uint16_t dispW,
                               uint16_t dispH, uint16_t scOffX, uint16_t scOffY, uint16_t scSpX, uint16_t scSpY,
                               int16_t scStX, int16_t scStY);
uint8_t FIA_DeleteScrollBuffer(uint8_t id);
void FIA_UpdateNextFreeScrollBufferIndex(void);
uint8_t FIA_SetBitmapDestinationBuffer(uint8_t id);
uint8_t FIA_GetBitmapDestinationBuffer(void);
void FIA_SetMaskEnabled(uint8_t state);
uint8_t FIA_GetMaskEnabled(void);
void FIA_RegulateTempAndHumidity(void);