#include "communication.h"
#include "usbd_cdc_if.h"

#include <string.h>

rxState_t curRxState = RX_IDLE;

void cdc_receiveData(uint8_t* buf, uint32_t len) {
    switch (curRxState) {
        case RX_IDLE: {
            // Receive first chunk including length bytes
            if (len < 2) {
                // Not enough data, just ignore it
                return;
            }
            curRxLenExpected = (((uint16_t)buf[0] << 8) | buf[1]);
            curRxState = RX_ACTIVE;
            curRxLenReceived = 0;
            cdc_receiveData(buf + 2, len - 2);
            break;
        }

        case RX_ACTIVE: {
            uint16_t rxLenDelta = curRxLenExpected - curRxLenReceived;
            if (len < rxLenDelta) {
                memcpy(bitmapBuffer + curRxLenReceived, buf, len);
                curRxLenReceived += len;
            } else {
                memcpy(bitmapBuffer + curRxLenReceived, buf, rxLenDelta);
                curRxLenReceived += rxLenDelta;
            }
            if (curRxLenReceived >= curRxLenExpected) {
                // All done
                curRxState = RX_IDLE;
                curRxLenExpected = 0;
                curRxLenReceived = 0;
            }
            break;
        }
    }
}