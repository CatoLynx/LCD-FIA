#pragma once

/*
  sht21.h / sht21.c - Library for the SHT21 I²C Temperature and Humidity sensor
*/

#include <stdint.h>

// Global definitions
#define I2C_ADDR_SENS_SHT21 (0x40 << 1)
#define SHT21_CMD_CONV_TEMP_NO_HOLD 0xF3
#define SHT21_CMD_CONV_RH_NO_HOLD 0xF5

// Typedefs
typedef enum SHT21_Comm_State { IDLE, CONV_STARTED_TEMP, CONV_STARTED_RH, READOUT_STARTED_TEMP, READOUT_STARTED_RH } SHT21_Comm_State_t;

// Function prototypes
void SHT21_StartTempConversion(void);
void SHT21_StartHumidityConversion(void);
void SHT21_StartReadout(void);
void SHT21_HandleCommunication(void);
double SHT21_GetTemperature(void);
double SHT21_GetHumidity(void);
