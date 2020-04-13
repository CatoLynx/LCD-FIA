#pragma once

/*
  aditech.h / aditech.c - LCD driver library

  This library contains the code to drive the LCD modules.
  It also contains functions to convert regular bitmap data
  into the format required by the LCD modules.
*/

#include "fia.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>

/*
  TERMINOLOGY:

  Half panel = one logical LCD unit, consistion og one data input, one data output and on-board LCD drivers
  Panel = one physical LCD unit, consisting of two completely electrically separate half panels rotated 180 degrees
  Backplane = one of four electrical groups of pixels in one half panel, part of the multiplexing process
  Bitmap buffer = high level buffer containing image data without any additional control bytes
  LCD buffer = low level buffer containing the image data stream to be sent to the LCD, including all control bytes
  LCD bus = a chain of half panels connected to one data output
*/

#define MUX4 (0b10)       // Control bits for 1:4 multiplex mode
#define RAM1 (0)          // Control bit for writing to RAM1
#define RAM2 (1 << 2)     // Control bit for writing to RAM2
#define COM0 (0)          // Control address for common 0
#define COM1 (0b100 << 3) // Control address for common 1
#define COM2 (0b010 << 3) // Control address for common 2
#define COM3 (0b110 << 3) // Control address for common 3
#define PX_SET (1 << 6)   // Control bit for setting all pixels
#define PX_BLANK (1 << 7) // Control bit for blanking all pixels

#define LSBFIRST 0
#define MSBFIRST 1

// Includes added control and offset bytes
#define BACKPLANE_NUM_LCD_BYTES 120
#define NUM_BACKPLANES 4
#define HALF_PANEL_NUM_LCD_BYTES (BACKPLANE_NUM_LCD_BYTES * NUM_BACKPLANES)

// Size of the raw LCD data buffer (per LCD bus)
#define LCD_BUF_SIZE (HALF_PANEL_NUM_LCD_BYTES * NUM_HALF_PANELS)

// Array containing the logical order to address the backplanes in, according to datasheet
const uint8_t BACKPLANES[NUM_BACKPLANES];

// Transmission status data per LCD bus
uint8_t curTransmittedBackplaneIndex[NUM_LCD_BUSES];
uint8_t transmitActive[NUM_LCD_BUSES];
uint8_t* curLCDData[NUM_LCD_BUSES];

// Function prototypes
uint16_t LCD_ConvertBitmap(uint8_t* dst, uint8_t* src, uint8_t panelRowOffset, uint8_t ramSel);
uint8_t LCD_IsTransmitActive(FIA_LCD_Bus_t row);
uint8_t LCD_TransmitBitmap(uint8_t* data, uint8_t numHalfPanels, FIA_LCD_Bus_t row);