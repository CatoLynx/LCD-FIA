#include <stdint.h>
#include "aditech.h"

typedef enum {
  RX_IDLE,
  RX_ACTIVE
} rxState_t;
rxState_t curRxState;
uint16_t curRxLenExpected;
uint16_t curRxLenReceived;
uint8_t bitmapBuffer[BITMAP_BUF_SIZE];

void cdc_receiveData(uint8_t* buf, uint32_t len);