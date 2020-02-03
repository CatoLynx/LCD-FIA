#include <stdint.h>
#include "stm32f1xx_hal.h"

#define MUX4 (0b10)       // Control bits for 1:4 multiplex mode
#define RAM1 (0)          // Control bit for writing to RAM1
#define RAM2 (1 << 2)     // Control bit for writing to RAM2
#define COM0 (0)          // Control address for common 0
#define COM1 (0b100 << 3) // Control address for common 1
#define COM2 (0b010 << 3) // Control address for common 2
#define COM3 (0b110 << 3) // Control address for common 3
#define PX_SET (1 << 6)      // Control bit for setting all pixels
#define PX_BLANK (1 << 7)    // Control bit for blanking all pixels

#define LSBFIRST 0
#define MSBFIRST 1

// How many panels are daisy-chained.
// This assumes that first all top halves of each display
// are chained, then all bottom halves.
#define NUM_PANELS    6
#define NUM_HALF_PANELS (NUM_PANELS * 2)
#define PANEL_WIDTH  96
#define PANEL_HEIGHT 64
#define HALF_PANEL_NUM_BITMAP_BYTES 384

// Includes added control and offset bytes
#define BACKPLANE_NUM_LCD_BYTES 120
#define NUM_BACKPLANES 4
#define HALF_PANEL_NUM_LCD_BYTES (BACKPLANE_NUM_LCD_BYTES * NUM_BACKPLANES)
#define LCD_BUF_SIZE (HALF_PANEL_NUM_LCD_BYTES * NUM_HALF_PANELS)
#define BITMAP_BUF_SIZE (HALF_PANEL_NUM_BITMAP_BYTES * NUM_HALF_PANELS)

const uint8_t BACKPLANES[NUM_BACKPLANES];
uint8_t curTransmittedBackplaneIndex;
uint8_t transmitActive;

uint8_t* curLCDData;

uint16_t convertBitmap(uint8_t* dst, uint8_t* src, uint8_t numHalfPanels, uint8_t ramSel);
uint8_t isTransmitActive();
uint8_t transmitBitmap(uint8_t* data, uint8_t numHalfPanels);
void LCDTransmitCompleteCallback(DMA_HandleTypeDef *hdma);