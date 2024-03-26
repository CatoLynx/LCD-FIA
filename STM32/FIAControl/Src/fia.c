#include "fia.h"
#include "aditech.h"
#include "ds18b20.h"
#include "heap.h"
#include "sht21.h"
#include "spi.h"
#include "stm32f4xx_hal.h"
#include "uart_protocol.h"
#include "util.h"
#include <math.h>
#include <string.h>


// Variables for brightness sensors
uint32_t adcRingBufferIndex = 0;
uint32_t adcValues[ADC_NUM_CHANNELS] = {0};
uint32_t adcRingBuffer[ADC_NUM_CHANNELS * ADC_AVG_COUNT] = {0};
uint32_t adcAverages[ADC_NUM_CHANNELS] = {0};
uint16_t lcdContrast[2] = {2048, 2048};
uint8_t updateLCDContrastFlag = 0;
uint16_t envBrightness[2] = {0, 0};
int16_t backlightBaseBrightness[2] = {2048, 2048};
uint16_t backlightBrightness[2] = {0, 0};
uint8_t updateBacklightBrightnessFlag = 0;
uint8_t firstADCReadFlag = 1;
uint8_t firstADCAverageFlag = 0;

// Variables for receiving bitmap data from the high-level controller
uint8_t FIA_staticBufferSideA[BITMAP_BUF_SIZE] = {0};
uint8_t FIA_staticBufferSideB[BITMAP_BUF_SIZE] = {0};
uint8_t FIA_dynamicBufferSideA[BITMAP_BUF_SIZE] = {0};
uint8_t FIA_dynamicBufferSideB[BITMAP_BUF_SIZE] = {0};
uint8_t FIA_maskBufferSideA[BITMAP_BUF_SIZE] = {0};
uint8_t FIA_maskBufferSideB[BITMAP_BUF_SIZE] = {0};
uint8_t FIA_displayBufferSideA[BITMAP_BUF_SIZE] = {0};
uint8_t FIA_displayBufferSideB[BITMAP_BUF_SIZE] = {0};
uint8_t FIA_bitmapRxActive = 0;
uint8_t* FIA_bitmapRxBuf = NULL;
uint8_t FIA_bitmapRxBufID = SIDE_BOTH;
uint8_t FIA_bitmapRxBoth = 0;
uint16_t FIA_bitmapRxLen = 0;
uint8_t FIA_maskEnabled = 0;

// Variables for scroll buffers
FIA_Scroll_Buffer_t FIA_scrollBuffers[MAX_SCROLL_BUFFERS] = {0};
uint8_t FIA_scrollBufferCount = 0;
int8_t FIA_nextFreeScrollBufferIndex = 0;

// Variables for temperature sensors
double FIA_tempSensorValues[4] = {0};
uint8_t FIA_circulationFansOverrideBLBallast = 0;
uint8_t FIA_circulationFansOverrideHeatersTemp = 0;
uint8_t FIA_circulationFansOverrideHeatersHumidity = 0;

// Status variables for selected RAM per LCD bus
uint8_t FIA_LCDcurrentRAM[NUM_LCD_BUSES] = {0};

FIA_Side_t oldDoorStatus = SIDE_NONE;

extern uint8_t* lcdDataBuffers[4];

void FIA_Init(void) {
    HAL_TIM_Base_Start(&DELAY_US_TIMER);
    HAL_TIM_Base_Start(&LCD_FRAME_TIMER);
    HAL_TIM_PWM_Start(&LCD_FRAME_TIMER, TIM_CHANNEL_1);
    HAL_TIM_Base_Start(&ADC_UPDATE_TIMER);
    HAL_TIM_Base_Start_IT(&ADC_UPDATE_TIMER);
    HAL_TIM_Base_Start(&DS18B20_UPDATE_TIMER);
    HAL_TIM_Base_Start_IT(&DS18B20_UPDATE_TIMER);
    HAL_TIM_Base_Start(&SCROLL_UPDATE_TIMER);
    HAL_TIM_Base_Start_IT(&SCROLL_UPDATE_TIMER);
    HAL_DAC_Start(&LCD_CONTRAST_DAC, DAC1_CHANNEL_1);
    HAL_DAC_Start(&LCD_CONTRAST_DAC, DAC1_CHANNEL_2);
    HAL_ADC_Start_DMA(&ENV_BRIGHTNESS_ADC, adcValues, ADC_NUM_CHANNELS);

    heapInit();

    FIA_InitI2CDACs();

    FIA_SetStatusLED(1, 0);
    FIA_SetStatusLED(2, 0);

    UART_StartRxRingBuffer();

    FIA_SetBitmapDestinationBuffer(SIDE_BOTH);

#ifdef SPLASH_ENABLED
    memcpy(FIA_staticBufferSideA, SPLASH, SPLASH_SIZE);
    memcpy(FIA_staticBufferSideB, SPLASH, SPLASH_SIZE);
#else
    memset(FIA_staticBufferSideA, 0xFF, BITMAP_BUF_SIZE);
    memset(FIA_staticBufferSideB, 0xFF, BITMAP_BUF_SIZE);
#endif

    memset(FIA_maskBufferSideA, 0xFF, BITMAP_BUF_SIZE);
    memset(FIA_maskBufferSideB, 0xFF, BITMAP_BUF_SIZE);

    FIA_SetLCDContrast(SIDE_A, 2048);
    FIA_SetLCDContrast(SIDE_B, 2048);

    FIA_SetBacklightBrightness(SIDE_A, 0);
    FIA_SetBacklightBrightness(SIDE_B, 0);

    FIA_SetBacklight(1);
}

void FIA_InitI2CDACs(void) {
    uint8_t buf[3];
    buf[0] = 0b01000000; // Cmd: Write volatile mem, conf: ref=VDD, no power down, gain=1
    buf[1] = 0x00;
    buf[2] = 0x00;
    HAL_I2C_Master_Transmit(&PERIPHERALS_I2C, I2C_ADDR_DAC_BRIGHTNESS_A, buf, 3, I2C_TIMEOUT);
    HAL_I2C_Master_Transmit(&PERIPHERALS_I2C, I2C_ADDR_DAC_BRIGHTNESS_B, buf, 3, I2C_TIMEOUT);
}

void FIA_MainLoop(void) {
    UART_HandleProtocol();

    if (updateLCDContrastFlag) {
        FIA_UpdateLCDContrast();
        updateLCDContrastFlag = 0;
    }

    FIA_Side_t doorStatus = FIA_GetDoors();
    if (updateBacklightBrightnessFlag || (doorStatus != oldDoorStatus)) {
        // Auto-adjust brightness or set to minimum if door is open
        FIA_SetBacklightBrightness(
            SIDE_A, (doorStatus & SIDE_A) ? 0 : FIA_CalculateBacklightBrightness(SIDE_A, FIA_GetEnvBrightness(SIDE_A)));
        FIA_SetBacklightBrightness(
            SIDE_B, (doorStatus & SIDE_B) ? 0 : FIA_CalculateBacklightBrightness(SIDE_B, FIA_GetEnvBrightness(SIDE_B)));
        updateBacklightBrightnessFlag = 0;
        oldDoorStatus = doorStatus;
    }

    FIA_UpdateScrollPositions();
    FIA_UpdateDisplayBuffers();
    FIA_UpdateDisplay(BUS_1);
    FIA_UpdateDisplay(BUS_2);
    FIA_UpdateDisplay(BUS_3);
    FIA_UpdateDisplay(BUS_4);
}

void FIA_UpdateScrollPositions(void) {
    // Called via timer interrupt.
    // Update all scroll positions as necessary
    for (uint8_t i = 0; i < MAX_SCROLL_BUFFERS; i++) {
        FIA_Scroll_Buffer_t* buf = &FIA_scrollBuffers[i];
        if (!buf->occupied)
            continue;
        buf->scrollTickCntX++;
        buf->scrollTickCntY++;
        if (buf->scrollSpeedX != 0 && buf->scrollTickCntX >= buf->scrollSpeedX) {
            FIA_ScrollBufferRelative(buf, buf->scrollStepX, 0);
            buf->scrollTickCntX = 0;
        }
        if (buf->scrollSpeedY != 0 && buf->scrollTickCntY >= buf->scrollSpeedY) {
            FIA_ScrollBufferRelative(buf, 0, buf->scrollStepY);
            buf->scrollTickCntY = 0;
        }
    }
}

void FIA_ScrollBufferRelative(FIA_Scroll_Buffer_t* buf, int16_t xStep, int16_t yStep) {
    // Scroll a scroll buffer by the specified number of steps
    if (!buf->occupied)
        return;
    if (xStep != 0) {
        while (-xStep > buf->scrollOffsetX)
            xStep += buf->intW;
        buf->scrollOffsetX += xStep;
        buf->scrollOffsetX %= buf->intW;
    }
    if (yStep != 0) {
        while (-yStep > buf->scrollOffsetY)
            yStep += buf->intH;
        buf->scrollOffsetY += yStep;
        buf->scrollOffsetY %= buf->intH;
    }
}

void FIA_RenderScrollBuffer(FIA_Scroll_Buffer_t buf) {
    if (!buf.occupied || buf.side == SIDE_NONE)
        return;
    uint16_t intHBytes = buf.intH / 8;
    uint8_t baseBitOffset = buf.dispY % 8;
    int16_t scrollBitOffsetTmp = buf.scrollOffsetY - baseBitOffset;
    int16_t byteOffset = scrollBitOffsetTmp / 8;
    if (scrollBitOffsetTmp < 0)
        byteOffset -= 1;
    uint8_t scrollBitOffset = scrollBitOffsetTmp - (byteOffset * 8);
    uint8_t baseBitOffsetCmpl = 8 - baseBitOffset;
    uint8_t scrollBitOffsetCmpl = 8 - scrollBitOffset;
    uint8_t sbufWinAligned = (scrollBitOffset + baseBitOffset) % 8 == 0;
    uint8_t winDispAligned = baseBitOffset == 0;
    uint8_t sbufDispAligned = scrollBitOffset == 0;
    uint8_t renderA = buf.side == SIDE_A || buf.side == SIDE_BOTH;
    uint8_t renderB = buf.side == SIDE_B || buf.side == SIDE_BOTH;
    uint8_t *dispA, *dispB;
    if (FIA_maskEnabled) {
        dispA = FIA_dynamicBufferSideA;
        dispB = FIA_dynamicBufferSideB;
    } else {
        dispA = FIA_staticBufferSideA;
        dispB = FIA_staticBufferSideB;
    }

    for (uint16_t x = 0; x < BITMAP_BUF_W_PX; x++) {
        // Continue if we're outside the display window
        if (x < buf.dispX || x > (buf.dispX + buf.dispW - 1))
            continue;

        uint16_t dispBaseIdx = x * BITMAP_BUF_H_BYTES;
        uint16_t sbufBaseIdx = ((x - buf.dispX + buf.scrollOffsetX) % buf.intW) * intHBytes;
        uint16_t renderByteIdx = 0;

        for (uint16_t yByte = 0; yByte < BITMAP_BUF_H_BYTES; yByte++) {
            uint16_t y1 = yByte * 8;
            uint16_t y2 = y1 + 7;

            // Y range check: Does the display window intersect the current byte?
            if (!((y1 <= buf.dispY + buf.dispH - 1) && (buf.dispY <= y2)))
                continue;

            uint16_t dispIdx = dispBaseIdx + yByte;
            uint16_t sbufOffset = renderByteIdx + byteOffset;
            uint16_t sbufIdx = sbufBaseIdx + (sbufOffset % intHBytes);
            uint16_t sbufNextIdx = sbufBaseIdx + ((sbufOffset + 1) % intHBytes);
            uint16_t numRenderBytes = ceil((double)(baseBitOffset + buf.dispH) / 8.0);
            uint16_t nextFullByteH = (renderByteIdx + 1) * 8;
            uint8_t isFirstByte = renderByteIdx == 0;
            uint8_t isLastByte = renderByteIdx >= numRenderBytes - 1;

            if (sbufWinAligned && winDispAligned) {
                // Everything is aligned, bytes can just be copied.
                // Last byte might have to be combined with display buffer
                // depending on window height.
                if (isLastByte) {
                    // Last byte - Combine existing display content
                    // with scroll buffer content
                    uint8_t keepBits = nextFullByteH - buf.dispH;
                    if (renderA)
                        dispA[dispIdx] =
                            (dispA[dispIdx] & (0xFF << (8 - keepBits))) | (buf.buf[sbufIdx] & (0xFF >> keepBits));
                    if (renderB)
                        dispB[dispIdx] =
                            (dispB[dispIdx] & (0xFF << (8 - keepBits))) | (buf.buf[sbufIdx] & (0xFF >> keepBits));
                } else {
                    // Any other byte - Copy straight from the scroll buffer
                    if (renderA)
                        dispA[dispIdx] = buf.buf[sbufIdx];
                    if (renderB)
                        dispB[dispIdx] = buf.buf[sbufIdx];
                }
            } else if (sbufWinAligned && !winDispAligned) {
                // Scroll buffer is aligned to window
                // but window is not aligned to display.
                // First byte will be combined with display buffer,
                // last byte might be combined depending on window height.
                // Scroll buffer bytes will be combined.
                if (isFirstByte && isLastByte) {
                    // First and last byte - display window is only one byte high or less
                    // Combine existing display content with scroll buffer content
                    uint8_t keepBits = nextFullByteH - baseBitOffset - buf.dispH;
                    uint8_t keepMask = (0xFF >> baseBitOffsetCmpl) | (0xFF << (8 - keepBits));
                    if (renderA)
                        dispA[dispIdx] =
                            (dispA[dispIdx] & keepMask) | ((buf.buf[sbufNextIdx] << scrollBitOffsetCmpl) & ~keepMask);
                    if (renderB)
                        dispB[dispIdx] =
                            (dispB[dispIdx] & keepMask) | ((buf.buf[sbufNextIdx] << scrollBitOffsetCmpl) & ~keepMask);
                } else if (isFirstByte) {
                    // First byte - Combine disp & 1 sbuf
                    uint8_t keepMask = 0xFF >> baseBitOffsetCmpl;
                    if (renderA)
                        dispA[dispIdx] =
                            (dispA[dispIdx] & keepMask) | ((buf.buf[sbufNextIdx] << scrollBitOffsetCmpl) & ~keepMask);
                    if (renderB)
                        dispB[dispIdx] =
                            (dispB[dispIdx] & keepMask) | ((buf.buf[sbufNextIdx] << scrollBitOffsetCmpl) & ~keepMask);
                } else if (isLastByte) {
                    // Last byte - Combine disp & 2 sbuf
                    uint8_t keepBits = nextFullByteH - baseBitOffset - buf.dispH;
                    uint8_t keepMask = 0xFF << (8 - keepBits);
                    uint8_t sbufMask = 0xFF >> scrollBitOffset;
                    if (renderA)
                        dispA[dispIdx] = (dispA[dispIdx] & keepMask) |
                                         ((buf.buf[sbufIdx] >> scrollBitOffset) & sbufMask & ~keepMask) |
                                         ((buf.buf[sbufNextIdx] << scrollBitOffsetCmpl) & ~sbufMask & ~keepMask);
                    if (renderB)
                        dispB[dispIdx] = (dispB[dispIdx] & keepMask) |
                                         ((buf.buf[sbufIdx] >> scrollBitOffset) & sbufMask & ~keepMask) |
                                         ((buf.buf[sbufNextIdx] << scrollBitOffsetCmpl) & ~sbufMask & ~keepMask);
                } else {
                    // Inbetween byte - Combine 2 sbuf
                    uint8_t sbufMask = 0xFF >> scrollBitOffset;
                    if (renderA)
                        dispA[dispIdx] = ((buf.buf[sbufIdx] >> scrollBitOffset) & sbufMask) |
                                         ((buf.buf[sbufNextIdx] << scrollBitOffsetCmpl) & ~sbufMask);
                    if (renderB)
                        dispB[dispIdx] = ((buf.buf[sbufIdx] >> scrollBitOffset) & sbufMask) |
                                         ((buf.buf[sbufNextIdx] << scrollBitOffsetCmpl) & ~sbufMask);
                }
            } else if (!sbufWinAligned && winDispAligned) {
                // Scroll buffer is not aligned to the window
                // but the window is aligned to the display.
                // Last byte will be combined with display buffer
                // depending on window height.
                // Scroll buffer bytes will be combined even in the first byte.
                if (isLastByte) {
                    // Last byte - Combine disp + 2 sbuf
                    uint8_t keepBits = nextFullByteH - buf.dispH;
                    uint8_t keepMask = 0xFF << (8 - keepBits);
                    uint8_t sbufMask = 0xFF >> scrollBitOffset;
                    if (renderA)
                        dispA[dispIdx] = (dispA[dispIdx] & keepMask) |
                                         ((buf.buf[sbufIdx] >> scrollBitOffset) & sbufMask & ~keepMask) |
                                         ((buf.buf[sbufNextIdx] << scrollBitOffsetCmpl) & ~sbufMask & ~keepMask);
                    if (renderB)
                        dispB[dispIdx] = (dispB[dispIdx] & keepMask) |
                                         ((buf.buf[sbufIdx] >> scrollBitOffset) & sbufMask & ~keepMask) |
                                         ((buf.buf[sbufNextIdx] << scrollBitOffsetCmpl) & ~sbufMask & ~keepMask);
                } else {
                    // Any other byte - Combine 2 sbuf
                    uint8_t sbufMask = 0xFF >> scrollBitOffset;
                    if (renderA)
                        dispA[dispIdx] = ((buf.buf[sbufIdx] >> scrollBitOffset) & sbufMask) |
                                         ((buf.buf[sbufNextIdx] << scrollBitOffsetCmpl) & ~sbufMask);
                    if (renderB)
                        dispB[dispIdx] = ((buf.buf[sbufIdx] >> scrollBitOffset) & sbufMask) |
                                         ((buf.buf[sbufNextIdx] << scrollBitOffsetCmpl) & ~sbufMask);
                }
            } else if (!sbufWinAligned && !winDispAligned && sbufDispAligned) {
                // Scroll buffer is not aligned to the window
                // and the window is not aligned to the display,
                // but the scroll buffer is aligned to the display.
                // First byte will be combined with display buffer,
                // Last byte will be combined with display buffer
                // depending on window height.
                // Scroll buffer bytes will not be combined.
                if (isFirstByte && isLastByte) {
                    // First and last byte - display window is only one byte high or less
                    // Combine disp + 1 sbuf
                    uint8_t keepBits = nextFullByteH - baseBitOffset - buf.dispH;
                    uint8_t keepMask = (0xFF >> baseBitOffsetCmpl) | (0xFF << (8 - keepBits));
                    if (renderA)
                        dispA[dispIdx] = (dispA[dispIdx] & keepMask) | (buf.buf[sbufIdx] & ~keepMask);
                    if (renderB)
                        dispB[dispIdx] = (dispB[dispIdx] & keepMask) | (buf.buf[sbufIdx] & ~keepMask);
                } else if (isFirstByte) {
                    // First byte - Combine disp + 1 sbuf
                    uint8_t keepMask = 0xFF >> baseBitOffsetCmpl;
                    if (renderA)
                        dispA[dispIdx] = (dispA[dispIdx] & keepMask) | (buf.buf[sbufIdx] & ~keepMask);
                    if (renderB)
                        dispB[dispIdx] = (dispB[dispIdx] & keepMask) | (buf.buf[sbufIdx] & ~keepMask);
                } else if (isLastByte) {
                    // Last byte - Combine disp + 1 sbuf
                    uint8_t keepBits = nextFullByteH - baseBitOffset - buf.dispH;
                    uint8_t keepMask = 0xFF << (8 - keepBits);
                    if (renderA)
                        dispA[dispIdx] = (dispA[dispIdx] & keepMask) | (buf.buf[sbufIdx] & ~keepMask);
                    if (renderB)
                        dispB[dispIdx] = (dispB[dispIdx] & keepMask) | (buf.buf[sbufIdx] & ~keepMask);
                } else {
                    // Any other byte - Copy
                    if (renderA)
                        dispA[dispIdx] = buf.buf[sbufIdx];
                    if (renderB)
                        dispB[dispIdx] = buf.buf[sbufIdx];
                }
            } else if (!sbufWinAligned && !winDispAligned && !sbufDispAligned) {
                // Scroll buffer is not aligned to the window
                // and the window is not aligned to the display,
                // and the scroll buffer is not aligned to the display.
                // First byte will be combined with display buffer,
                // Last byte will be combined with display buffer
                // depending on window height.
                // Scroll buffer bytes will be combined.
                if (isFirstByte && isLastByte) {
                    // First and last byte - display window is only one byte high or less
                    // Combine disp + 2 sbuf
                    uint8_t keepBits = nextFullByteH - baseBitOffset - buf.dispH;
                    uint8_t keepMask = (0xFF >> baseBitOffsetCmpl) | (0xFF << (8 - keepBits));
                    uint8_t sbufMask = 0xFF >> scrollBitOffset;
                    if (renderA)
                        dispA[dispIdx] = (dispA[dispIdx] & keepMask) |
                                         ((buf.buf[sbufIdx] >> scrollBitOffset) & sbufMask & ~keepMask) |
                                         ((buf.buf[sbufNextIdx] << scrollBitOffsetCmpl) & ~sbufMask & ~keepMask);
                    if (renderB)
                        dispB[dispIdx] = (dispB[dispIdx] & keepMask) |
                                         ((buf.buf[sbufIdx] >> scrollBitOffset) & sbufMask & ~keepMask) |
                                         ((buf.buf[sbufNextIdx] << scrollBitOffsetCmpl) & ~sbufMask & ~keepMask);
                } else if (isFirstByte) {
                    // First byte - Combine disp + 2 sbuf
                    uint8_t keepMask = 0xFF >> baseBitOffsetCmpl;
                    uint8_t sbufMask = 0xFF >> scrollBitOffset;
                    if (renderA)
                        dispA[dispIdx] = (dispA[dispIdx] & keepMask) |
                                         ((buf.buf[sbufIdx] >> scrollBitOffset) & sbufMask & ~keepMask) |
                                         ((buf.buf[sbufNextIdx] << scrollBitOffsetCmpl) & ~sbufMask & ~keepMask);
                    if (renderB)
                        dispB[dispIdx] = (dispB[dispIdx] & keepMask) |
                                         ((buf.buf[sbufIdx] >> scrollBitOffset) & sbufMask & ~keepMask) |
                                         ((buf.buf[sbufNextIdx] << scrollBitOffsetCmpl) & ~sbufMask & ~keepMask);
                } else if (isLastByte) {
                    // Last byte - Combine disp + 2 sbuf
                    uint8_t keepBits = nextFullByteH - baseBitOffset - buf.dispH;
                    uint8_t keepMask = 0xFF << (8 - keepBits);
                    uint8_t sbufMask = 0xFF >> scrollBitOffset;
                    if (renderA)
                        dispA[dispIdx] = (dispA[dispIdx] & keepMask) |
                                         ((buf.buf[sbufIdx] >> scrollBitOffset) & sbufMask & ~keepMask) |
                                         ((buf.buf[sbufNextIdx] << scrollBitOffsetCmpl) & ~sbufMask & ~keepMask);
                    if (renderB)
                        dispB[dispIdx] = (dispB[dispIdx] & keepMask) |
                                         ((buf.buf[sbufIdx] >> scrollBitOffset) & sbufMask & ~keepMask) |
                                         ((buf.buf[sbufNextIdx] << scrollBitOffsetCmpl) & ~sbufMask & ~keepMask);
                } else {
                    // Any other byte - Combine 2 sbuf
                    uint8_t sbufMask = 0xFF >> scrollBitOffset;
                    if (renderA)
                        dispA[dispIdx] = ((buf.buf[sbufIdx] >> scrollBitOffset) & sbufMask) |
                                         ((buf.buf[sbufNextIdx] << scrollBitOffsetCmpl) & ~sbufMask);
                    if (renderB)
                        dispB[dispIdx] = ((buf.buf[sbufIdx] >> scrollBitOffset) & sbufMask) |
                                         ((buf.buf[sbufNextIdx] << scrollBitOffsetCmpl) & ~sbufMask);
                }
            } else {
                // This should never happen.
                // Ignore if it does.
            }

            renderByteIdx++;
        }
    }
}

void FIA_RenderScrollBuffers(void) {
    if (FIA_scrollBufferCount == 0)
        return;
    for (uint8_t i = 0; i < MAX_SCROLL_BUFFERS; i++) {
        FIA_RenderScrollBuffer(FIA_scrollBuffers[i]);
    }
}

void FIA_UpdateDisplayBuffers(void) {
    // Render scroll buffers
    FIA_RenderScrollBuffers();

    if (FIA_maskEnabled) {
        // Blend static and dynamic buffers
        for (uint16_t i = 0; i < BITMAP_BUF_SIZE; i++) {
            FIA_displayBufferSideA[i] = (FIA_staticBufferSideA[i] & ~FIA_maskBufferSideA[i]) |
                                        (FIA_dynamicBufferSideA[i] & FIA_maskBufferSideA[i]);
            FIA_displayBufferSideB[i] = (FIA_staticBufferSideB[i] & ~FIA_maskBufferSideB[i]) |
                                        (FIA_dynamicBufferSideB[i] & FIA_maskBufferSideB[i]);
        }
    } else {
        memcpy(FIA_displayBufferSideA, FIA_staticBufferSideA, BITMAP_BUF_SIZE);
        memcpy(FIA_displayBufferSideB, FIA_staticBufferSideB, BITMAP_BUF_SIZE);
    }
}

void FIA_UpdateDisplay(FIA_LCD_Bus_t bus) {
    uint8_t panelRowOffset;
    uint8_t* bitmapBuffer;

    if (bus == BUS_1 || bus == BUS_3) {
        panelRowOffset = 0;
    } else {
        panelRowOffset = 1;
    }

    if (bus == BUS_1 || bus == BUS_2) {
        bitmapBuffer = FIA_displayBufferSideA;
    } else {
        bitmapBuffer = FIA_displayBufferSideB;
    }

    if (!LCD_IsTransmitActive(bus)) {
        LCD_ConvertBitmap(lcdDataBuffers[bus], bitmapBuffer, panelRowOffset, FIA_LCDcurrentRAM[bus]);
        LCD_TransmitBitmap(lcdDataBuffers[bus], NUM_HALF_PANELS, bus);
        if (FIA_LCDcurrentRAM[bus] == RAM1) {
            FIA_LCDcurrentRAM[bus] = RAM2;
        } else {
            FIA_LCDcurrentRAM[bus] = RAM1;
        }
    }
}

void FIA_StartBitmapReceive(void) {
    if (FIA_bitmapRxBuf == NULL || FIA_bitmapRxLen == 0) {
        return;
    }
    if (HAL_SPI_Receive_DMA(&BITMAP_DATA_SPI, FIA_bitmapRxBuf, FIA_bitmapRxLen) == HAL_OK) {
        FIA_SetStatusLED(2, 1);
        FIA_bitmapRxActive = 1;
    }
}

void FIA_AbortBitmapReceive(void) {
    if (!FIA_bitmapRxActive)
        return;
    if (HAL_SPI_Abort(&BITMAP_DATA_SPI) == HAL_OK) {
        FIA_SetStatusLED(2, 0);
        FIA_SetStatusLED(1, 0);
        FIA_bitmapRxActive = 0;
    }
}

void FIA_SetBacklightBrightness(FIA_Side_t side, uint16_t value) {
    uint8_t buf[2];
    buf[0] = (value >> 8) & 0x0F;
    buf[1] = value & 0xFF;
    switch (side) {
        case SIDE_A: {
            backlightBrightness[0] = value;
            HAL_I2C_Master_Transmit(&PERIPHERALS_I2C, I2C_ADDR_DAC_BRIGHTNESS_A, buf, 2, I2C_TIMEOUT);
            break;
        }
        case SIDE_B: {
            backlightBrightness[1] = value;
            HAL_I2C_Master_Transmit(&PERIPHERALS_I2C, I2C_ADDR_DAC_BRIGHTNESS_B, buf, 2, I2C_TIMEOUT);
            break;
        }
        case SIDE_BOTH: {
            backlightBrightness[0] = value;
            backlightBrightness[1] = value;
            HAL_I2C_Master_Transmit(&PERIPHERALS_I2C, I2C_ADDR_DAC_BRIGHTNESS_A, buf, 2, I2C_TIMEOUT);
            HAL_I2C_Master_Transmit(&PERIPHERALS_I2C, I2C_ADDR_DAC_BRIGHTNESS_B, buf, 2, I2C_TIMEOUT);
            break;
        }
        case SIDE_NONE: {
            break;
        }
    }
}

void FIA_UpdateLCDContrast(void) {
    HAL_DAC_SetValue(&hdac, DAC_LCD_CONTRAST_SIDE_A, DAC_ALIGN_12B_R, lcdContrast[0]);
    HAL_DAC_SetValue(&hdac, DAC_LCD_CONTRAST_SIDE_B, DAC_ALIGN_12B_R, lcdContrast[1]);
}

void FIA_SetLCDContrast(FIA_Side_t side, uint16_t value) {
    switch (side) {
        case SIDE_A: {
            lcdContrast[0] = value;
            break;
        }
        case SIDE_B: {
            lcdContrast[1] = value;
            break;
        }
        case SIDE_BOTH: {
            lcdContrast[0] = value;
            lcdContrast[1] = value;
            break;
        }
        case SIDE_NONE: {
            break;
        }
    }
    updateLCDContrastFlag = 1;
}

uint16_t FIA_GetLCDContrast(FIA_Side_t side) {
    if (side != SIDE_A && side != SIDE_B)
        return 0;
    return lcdContrast[side - 1];
}

void FIA_SetStatusLED(uint8_t number, uint8_t value) {
    if (number == 1) {
        HAL_GPIO_WritePin(STATUS_LED_1_GPIO_Port, STATUS_LED_1_Pin, !value);
    } else if (number == 2) {
        HAL_GPIO_WritePin(STATUS_LED_2_GPIO_Port, STATUS_LED_2_Pin, !value);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if (firstADCReadFlag) {
        firstADCReadFlag = 0;
        firstADCAverageFlag = 1;
    }
}

uint16_t FIA_GetEnvBrightness(FIA_Side_t side) {
    if (side != SIDE_A && side != SIDE_B)
        return 0;
    return envBrightness[side - 1];
}

void FIA_SetBacklightBaseBrightness(FIA_Side_t side, int16_t value) {
    switch (side) {
        case SIDE_A: {
            backlightBaseBrightness[0] = value;
            break;
        }
        case SIDE_B: {
            backlightBaseBrightness[1] = value;
            break;
        }
        case SIDE_BOTH: {
            backlightBaseBrightness[0] = value;
            backlightBaseBrightness[1] = value;
            break;
        }
        case SIDE_NONE: {
            break;
        }
    }
}

int16_t FIA_GetBacklightBaseBrightness(FIA_Side_t side) {
    if (side != SIDE_A && side != SIDE_B)
        return 0;
    return backlightBaseBrightness[side - 1];
}

uint16_t FIA_GetBacklightBrightness(FIA_Side_t side) {
    if (side != SIDE_A && side != SIDE_B)
        return 0;
    return backlightBrightness[side - 1];
}

uint16_t FIA_CalculateBacklightBrightness(FIA_Side_t side, uint16_t envBrt) {
    if (side != SIDE_A && side != SIDE_B)
        return 0;
    uint16_t result = (uint32_t)limitRange(envBrt + backlightBaseBrightness[side - 1] - 2048, 0, 4095);
    return result;
}

void FIA_UpdateADCValues(void) {
    // CH1, CH2 (Env Brightness) sensor range: 0.3 ... 0.65V (ADC values 370 ... 800)
    // CH3 (Internal Temperature) sensor range: ???
    uint16_t tmpVal = 0;

    for (uint8_t c = 0; c < ADC_NUM_CHANNELS; c++) {
        adcRingBuffer[adcRingBufferIndex++] = adcValues[c];
    }
    if (adcRingBufferIndex >= (ADC_NUM_CHANNELS * ADC_AVG_COUNT))
        adcRingBufferIndex = 0;
    if (firstADCAverageFlag) {
        for (uint8_t c = 0; c < ADC_NUM_CHANNELS; c++) {
            for (uint32_t i = c; i < ADC_AVG_COUNT * ADC_NUM_CHANNELS; i += ADC_NUM_CHANNELS) {
                adcRingBuffer[i] = adcValues[c];
            }
        }
        firstADCAverageFlag = 0;
    }
    avgInterleaved(adcRingBuffer, adcAverages, ADC_NUM_CHANNELS, ADC_AVG_COUNT);

    // 2 brightness sensors...
    for (uint8_t c = 0; c < 2; c++) {
        tmpVal = (uint16_t)mapRange(adcAverages[c], 370, 800, 0, 4095);
        if (tmpVal != envBrightness[c]) {
            envBrightness[c] = tmpVal;
            updateBacklightBrightnessFlag = 1;
        }
    }

    // ...and the internal temperature sensor
    FIA_tempSensorValues[3] =
        ((mapRange(adcAverages[2], 0, 4095, 0, 3.3) - TEMP_SENS_V25) / TEMP_SENS_AVG_SLOPE) + 25.0;
}

void FIA_SetBacklight(uint8_t status) {
    // Set backlight ON (1) or OFF (0)
    HAL_GPIO_WritePin(LIGHT_GPIO_Port, LIGHT_Pin, !!status);
}

uint8_t FIA_GetBacklight(void) {
    return HAL_GPIO_ReadPin(LIGHT_GPIO_Port, LIGHT_Pin);
}

void FIA_SetHeaters(uint8_t level) {
    // Set heaters OFF (0), ONE ON (1) or BOTH ON (2)
    if (level == 0) {
        HAL_GPIO_WritePin(HEATER_1_GPIO_Port, HEATER_1_Pin, 0);
        HAL_GPIO_WritePin(HEATER_2_GPIO_Port, HEATER_2_Pin, 0);
    } else if (level == 1) {
        HAL_GPIO_WritePin(HEATER_1_GPIO_Port, HEATER_1_Pin, 0);
        HAL_GPIO_WritePin(HEATER_2_GPIO_Port, HEATER_2_Pin, 1);
    } else {
        HAL_GPIO_WritePin(HEATER_1_GPIO_Port, HEATER_1_Pin, 1);
        HAL_GPIO_WritePin(HEATER_2_GPIO_Port, HEATER_2_Pin, 1);
    }
}

uint8_t FIA_GetHeaters(void) {
    uint8_t heater1 = HAL_GPIO_ReadPin(HEATER_1_GPIO_Port, HEATER_1_Pin);
    uint8_t heater2 = HAL_GPIO_ReadPin(HEATER_2_GPIO_Port, HEATER_2_Pin);
    return heater1 + heater2;
}

void FIA_SetCirculationFans(uint8_t level) {
    // Set circulation fans OFF (0), ONE ON (1) or BOTH ON (2)
    if (level == 0) {
        HAL_GPIO_WritePin(FAN_2_GPIO_Port, FAN_2_Pin, 0);
        HAL_GPIO_WritePin(FAN_3_GPIO_Port, FAN_3_Pin, 0);
    } else if (level == 1) {
        HAL_GPIO_WritePin(FAN_2_GPIO_Port, FAN_2_Pin, 0);
        HAL_GPIO_WritePin(FAN_3_GPIO_Port, FAN_3_Pin, 1);
    } else {
        HAL_GPIO_WritePin(FAN_2_GPIO_Port, FAN_2_Pin, 1);
        HAL_GPIO_WritePin(FAN_3_GPIO_Port, FAN_3_Pin, 1);
    }
}

uint8_t FIA_GetCirculationFans(void) {
    uint8_t fan1 = HAL_GPIO_ReadPin(FAN_2_GPIO_Port, FAN_2_Pin);
    uint8_t fan2 = HAL_GPIO_ReadPin(FAN_3_GPIO_Port, FAN_3_Pin);
    return fan1 + fan2;
}

void FIA_SetHeatExchangerFan(uint8_t status) {
    // Set heat exchanger fan ON (1) or OFF (0)
    HAL_GPIO_WritePin(FAN_1_GPIO_Port, FAN_1_Pin, !!status);
}

uint8_t FIA_GetHeatExchangerFan(void) {
    return HAL_GPIO_ReadPin(FAN_1_GPIO_Port, FAN_1_Pin);
}

void FIA_SetBacklightBallastFans(uint8_t status) {
    // Set backlight electronic ballast fans ON (1) or OFF (0)
    HAL_GPIO_WritePin(FAN_4_GPIO_Port, FAN_4_Pin, !!status);
}

uint8_t FIA_GetBacklightBallastFans(void) {
    return HAL_GPIO_ReadPin(FAN_4_GPIO_Port, FAN_4_Pin);
}

FIA_Side_t FIA_GetDoors(void) {
    // Get door open status: OPEN (1) or CLOSED (0)
    uint8_t doorA = !HAL_GPIO_ReadPin(DOOR_A_GPIO_Port, DOOR_A_Pin);
    uint8_t doorB = !HAL_GPIO_ReadPin(DOOR_B_GPIO_Port, DOOR_B_Pin);
    if (doorA && doorB)
        return SIDE_BOTH;
    if (doorA)
        return SIDE_A;
    if (doorB)
        return SIDE_B;
    return SIDE_NONE;
}

void FIA_StartExtTempSensorConv(void) {
    // Initiate a conversion in the external temperature sensors
    DS18B20_ConvertTemperature(TEMP_1_ONEWIRE_GPIO_Port, TEMP_1_ONEWIRE_Pin);
    DS18B20_ConvertTemperature(TEMP_2_ONEWIRE_GPIO_Port, TEMP_2_ONEWIRE_Pin);
}

void FIA_ReadTempSensors(void) {
    // Initiate a read from the temperature sensors
    double temp;
    temp = DS18B20_ReadTemperature(TEMP_1_ONEWIRE_GPIO_Port, TEMP_1_ONEWIRE_Pin);
    if (temp != DS18B20_ERROR)
        FIA_tempSensorValues[0] = temp;
    temp = DS18B20_ReadTemperature(TEMP_2_ONEWIRE_GPIO_Port, TEMP_2_ONEWIRE_Pin);
    if (temp != DS18B20_ERROR)
        FIA_tempSensorValues[1] = temp;
    FIA_tempSensorValues[2] = SHT21_GetTemperature();
    SHT21_HandleCommunication();
}

double FIA_GetTemperature(FIA_Temp_Sensor_t sensor) {
    // Get the value for the specified temperature sensor
    if (sensor < 0 || sensor > 3)
        return 0;
    return FIA_tempSensorValues[sensor];
}

double FIA_GetHumidity(void) {
    // Get the relative humidity of the inside air (in %)
    return SHT21_GetHumidity();
}

uint8_t FIA_CreateScrollBuffer(FIA_Side_t side, uint16_t dispX, uint16_t dispY, uint16_t dispW, uint16_t dispH,
                               uint16_t intW, uint16_t intH, uint16_t scOffX, uint16_t scOffY, uint16_t scSpX,
                               uint16_t scSpY, int16_t scStX, int16_t scStY) {
    if (FIA_scrollBufferCount >= MAX_SCROLL_BUFFERS || FIA_nextFreeScrollBufferIndex == -1) {
        return SCROLL_BUFFER_ERR_MASK | SCROLL_BUFFER_ERR_COUNT;
    }

    size_t bufSize = intW * roundUp(intH, 8) / 8;
    uint8_t* buf = malloc(bufSize);
    if (buf == NULL) {
        return SCROLL_BUFFER_ERR_MASK | SCROLL_BUFFER_ERR_SIZE;
    }
    memset(buf, 0x00, bufSize);

    FIA_Scroll_Buffer_t* scrollBuffer = &FIA_scrollBuffers[FIA_nextFreeScrollBufferIndex];
    scrollBuffer->occupied = 1;
    scrollBuffer->side = side;
    scrollBuffer->dispX = dispX;
    scrollBuffer->dispY = dispY;
    scrollBuffer->dispW = dispW;
    scrollBuffer->dispH = dispH;
    scrollBuffer->intW = intW;
    scrollBuffer->intH = intH;
    scrollBuffer->scrollOffsetX = scOffX;
    scrollBuffer->scrollOffsetY = scOffY;
    scrollBuffer->scrollSpeedX = scSpX;
    scrollBuffer->scrollSpeedY = scSpY;
    scrollBuffer->scrollStepX = scStX;
    scrollBuffer->scrollStepY = scStY;
    scrollBuffer->bufSize = bufSize;
    scrollBuffer->buf = buf;
    uint8_t ret = FIA_nextFreeScrollBufferIndex | SCROLL_BUFFER_ID_MASK;
    FIA_UpdateNextFreeScrollBufferIndex();
    FIA_scrollBufferCount++;
    return ret;
}

uint8_t FIA_UpdateScrollBuffer(uint8_t id, FIA_Side_t side, uint16_t dispX, uint16_t dispY, uint16_t dispW,
                               uint16_t dispH, uint16_t scOffX, uint16_t scOffY, uint16_t scSpX, uint16_t scSpY,
                               int16_t scStX, int16_t scStY) {
    if (!(id & SCROLL_BUFFER_ID_MASK)) {
        return 0;
    }
    uint8_t bufIdx = id - SCROLL_BUFFER_ID_MASK;
    if (bufIdx >= MAX_SCROLL_BUFFERS) {
        return 0;
    }
    FIA_Scroll_Buffer_t* scrollBuffer = &FIA_scrollBuffers[bufIdx];
    if (!scrollBuffer->occupied || scrollBuffer->buf == NULL) {
        return 0;
    }

    // Changes to display position or size require the dynamic buffers
    // to be cleared to clear out sticky scroll buffer residue
    uint8_t bufferResetNeeded = 0;

    if (side != 0xFF)
        scrollBuffer->side = side;
    bufferResetNeeded = 1;
    if (dispX != 0xFFFF)
        scrollBuffer->dispX = dispX;
    bufferResetNeeded = 1;
    if (dispY != 0xFFFF)
        scrollBuffer->dispY = dispY;
    bufferResetNeeded = 1;
    if (dispW != 0xFFFF)
        scrollBuffer->dispW = dispW;
    bufferResetNeeded = 1;
    if (dispH != 0xFFFF)
        scrollBuffer->dispH = dispH;
    bufferResetNeeded = 1;
    if (scOffX != 0xFFFF)
        scrollBuffer->scrollOffsetX = scOffX;
    if (scOffY != 0xFFFF)
        scrollBuffer->scrollOffsetY = scOffY;
    if (scSpX != 0xFFFF)
        scrollBuffer->scrollSpeedX = scSpX;
    if (scSpY != 0xFFFF)
        scrollBuffer->scrollSpeedY = scSpY;
    if (scStX != 0x7FFF)
        scrollBuffer->scrollStepX = scStX;
    if (scStY != 0x7FFF)
        scrollBuffer->scrollStepY = scStY;

    if (bufferResetNeeded) {
        memset(FIA_dynamicBufferSideA, 0x00, BITMAP_BUF_SIZE);
        memset(FIA_dynamicBufferSideB, 0x00, BITMAP_BUF_SIZE);
    }

    return 1;
}

uint8_t FIA_DeleteScrollBuffer(uint8_t id) {
    if (!(id & SCROLL_BUFFER_ID_MASK)) {
        return 0;
    }
    uint8_t bufIdx = id - SCROLL_BUFFER_ID_MASK;
    if (bufIdx >= MAX_SCROLL_BUFFERS) {
        return 0;
    }
    FIA_Scroll_Buffer_t* scrollBuffer = &FIA_scrollBuffers[bufIdx];
    if (!scrollBuffer->occupied || scrollBuffer->buf == NULL) {
        return 0;
    }
    if (FIA_bitmapRxBuf == scrollBuffer->buf) {
        // Prevent deletion of the currently active buffer
        return 0;
    }
    free(scrollBuffer->buf);
    scrollBuffer->occupied = 0;
    scrollBuffer->side = 0;
    scrollBuffer->dispX = 0;
    scrollBuffer->dispY = 0;
    scrollBuffer->dispW = 0;
    scrollBuffer->dispH = 0;
    scrollBuffer->intW = 0;
    scrollBuffer->intH = 0;
    scrollBuffer->scrollOffsetX = 0;
    scrollBuffer->scrollOffsetY = 0;
    scrollBuffer->scrollSpeedX = 0;
    scrollBuffer->scrollSpeedY = 0;
    scrollBuffer->scrollStepX = 0;
    scrollBuffer->scrollStepY = 0;
    scrollBuffer->scrollTickCntX = 0;
    scrollBuffer->scrollTickCntY = 0;
    scrollBuffer->bufSize = 0;
    scrollBuffer->buf = NULL;
    FIA_UpdateNextFreeScrollBufferIndex();
    if (FIA_scrollBufferCount > 0) FIA_scrollBufferCount--;

    // Deletion of scroll buffers requires the dynamic buffers
    // to be cleared to clear out sticky scroll buffer residue
    memset(FIA_dynamicBufferSideA, 0x00, BITMAP_BUF_SIZE);
    memset(FIA_dynamicBufferSideB, 0x00, BITMAP_BUF_SIZE);

    return 1;
}

void FIA_UpdateNextFreeScrollBufferIndex(void) {
    for (uint8_t i = 0; i < MAX_SCROLL_BUFFERS; i++) {
        if (!FIA_scrollBuffers[i].occupied) {
            FIA_nextFreeScrollBufferIndex = i;
            return;
        }
    }
    FIA_nextFreeScrollBufferIndex = -1;
}

uint8_t FIA_SetBitmapDestinationBuffer(uint8_t id) {
    if (id == SIDE_A) {
        FIA_bitmapRxBuf = FIA_staticBufferSideA;
        FIA_bitmapRxBoth = 0;
        FIA_bitmapRxLen = BITMAP_BUF_SIZE;
    } else if (id == SIDE_B) {
        FIA_bitmapRxBuf = FIA_staticBufferSideB;
        FIA_bitmapRxBoth = 0;
        FIA_bitmapRxLen = BITMAP_BUF_SIZE;
    } else if (id == SIDE_BOTH) {
        FIA_bitmapRxBuf = FIA_staticBufferSideA;
        FIA_bitmapRxBoth = 1;
        FIA_bitmapRxLen = BITMAP_BUF_SIZE;
    } else if (id & MASK_BUFFER_ID_MASK) {
        id &= ~MASK_BUFFER_ID_MASK;
        if (id == SIDE_A) {
            FIA_bitmapRxBuf = FIA_maskBufferSideA;
            FIA_bitmapRxBoth = 0;
            FIA_bitmapRxLen = BITMAP_BUF_SIZE;
        } else if (id == SIDE_B) {
            FIA_bitmapRxBuf = FIA_maskBufferSideB;
            FIA_bitmapRxBoth = 0;
            FIA_bitmapRxLen = BITMAP_BUF_SIZE;
        } else if (id == SIDE_BOTH) {
            FIA_bitmapRxBuf = FIA_maskBufferSideA;
            FIA_bitmapRxBoth = 1;
            FIA_bitmapRxLen = BITMAP_BUF_SIZE;
        }
    } else if (id & DYN_BUFFER_ID_MASK) {
        id &= ~DYN_BUFFER_ID_MASK;
        if (id == SIDE_A) {
            FIA_bitmapRxBuf = FIA_dynamicBufferSideA;
            FIA_bitmapRxBoth = 0;
            FIA_bitmapRxLen = BITMAP_BUF_SIZE;
        } else if (id == SIDE_B) {
            FIA_bitmapRxBuf = FIA_dynamicBufferSideB;
            FIA_bitmapRxBoth = 0;
            FIA_bitmapRxLen = BITMAP_BUF_SIZE;
        } else if (id == SIDE_BOTH) {
            FIA_bitmapRxBuf = FIA_dynamicBufferSideA;
            FIA_bitmapRxBoth = 1;
            FIA_bitmapRxLen = BITMAP_BUF_SIZE;
        }
    } else if (id & SCROLL_BUFFER_ID_MASK) {
        uint8_t bufIdx = id & ~SCROLL_BUFFER_ID_MASK;
        if (bufIdx >= MAX_SCROLL_BUFFERS) {
            return 0;
        }
        FIA_Scroll_Buffer_t* scrollBuffer = &FIA_scrollBuffers[bufIdx];
        if (!scrollBuffer->occupied || scrollBuffer->buf == NULL) {
            return 0;
        }
        FIA_bitmapRxBuf = scrollBuffer->buf;
        FIA_bitmapRxBoth = 0;
        FIA_bitmapRxLen = scrollBuffer->bufSize;
    } else {
        return 0;
    }

    FIA_bitmapRxBufID = id;
    return 1;
}

uint8_t FIA_GetBitmapDestinationBuffer(void) {
    return FIA_bitmapRxBufID;
}

void FIA_SetMaskEnabled(uint8_t state) {
    FIA_maskEnabled = !!state;
}

uint8_t FIA_GetMaskEnabled(void) {
    return FIA_maskEnabled;
}

void FIA_RegulateTempAndHumidity(void) {
    int8_t htrState = -1, blBallState = -1, circState = -1, exchState = -1;

    if (FIA_GetDoors() != SIDE_NONE) {
        // At least one door is open, disable all fans
        htrState = 0;
        blBallState = 0;
        circState = 0;
        exchState = 0;
    } else {
        if (FIA_tempSensorValues[AIRFLOW] <= HEATERS_FULL_TEMP) {
            htrState = 2;
            circState = 2;
            FIA_circulationFansOverrideHeatersTemp = 1;
        } else if (FIA_tempSensorValues[AIRFLOW] <= HEATERS_HALF_TEMP) {
            htrState = 1;
            circState = 2;
            FIA_circulationFansOverrideHeatersTemp = 1;
        } else if (FIA_tempSensorValues[AIRFLOW] >= HEATERS_OFF_TEMP) {
            htrState = 0;
            if (FIA_circulationFansOverrideHeatersTemp && !FIA_circulationFansOverrideBLBallast &&
                !FIA_circulationFansOverrideHeatersHumidity) {
                circState = 0;
            }
            FIA_circulationFansOverrideHeatersTemp = 0;
        }

        double humidity = FIA_GetHumidity();
        if (humidity >= HEATERS_FULL_HUMIDITY) {
            htrState = 2;
            circState = 2;
            FIA_circulationFansOverrideHeatersHumidity = 1;
        } else if (humidity >= HEATERS_HALF_HUMIDITY) {
            htrState = 1;
            circState = 2;
            FIA_circulationFansOverrideHeatersHumidity = 1;
        } else if (humidity <= HEATERS_OFF_HUMIDITY) {
            htrState = 0;
            if (FIA_circulationFansOverrideHeatersHumidity && !FIA_circulationFansOverrideBLBallast &&
                !FIA_circulationFansOverrideHeatersTemp) {
                circState = 0;
            }
            FIA_circulationFansOverrideHeatersHumidity = 0;
        }

        if (FIA_tempSensorValues[AIRFLOW] >= HEATERS_CUTOFF_TEMP) {
            htrState = 0;
            if (FIA_circulationFansOverrideHeatersTemp && !FIA_circulationFansOverrideBLBallast &&
                !FIA_circulationFansOverrideHeatersHumidity) {
                circState = 0;
            }
            FIA_circulationFansOverrideHeatersTemp = 0;
        }

        if (FIA_tempSensorValues[BL_BALL] >= BL_BALLAST_FANS_ON_TEMP) {
            blBallState = 1;
        } else if (FIA_tempSensorValues[BL_BALL] <= BL_BALLAST_FANS_OFF_TEMP) {
            blBallState = 0;
        }

        if (FIA_tempSensorValues[BL_BALL] >= CIRCULATION_FANS_BL_BALLAST_ON_TEMP) {
            circState = 2;
            FIA_circulationFansOverrideBLBallast = 1;
        } else if (FIA_tempSensorValues[BL_BALL] <= CIRCULATION_FANS_BL_BALLAST_OFF_TEMP) {
            if (FIA_circulationFansOverrideBLBallast && !FIA_circulationFansOverrideHeatersTemp &&
                !FIA_circulationFansOverrideHeatersHumidity) {
                circState = 0;
            }
            FIA_circulationFansOverrideBLBallast = 0;
        }

        if (!FIA_circulationFansOverrideBLBallast && !FIA_circulationFansOverrideHeatersTemp &&
            !FIA_circulationFansOverrideHeatersHumidity) {
            if (FIA_tempSensorValues[AIRFLOW] >= CIRCULATION_FANS_FULL_TEMP) {
                circState = 2;
            } else if (FIA_tempSensorValues[AIRFLOW] >= CIRCULATION_FANS_HALF_TEMP) {
                circState = 1;
            } else if (FIA_tempSensorValues[AIRFLOW] <= CIRCULATION_FANS_OFF_TEMP) {
                circState = 0;
            }
        }

        if (FIA_tempSensorValues[AIRFLOW] >= HEAT_EXCHANGER_FAN_ON_TEMP) {
            exchState = 1;
        } else if (FIA_tempSensorValues[AIRFLOW] <= HEAT_EXCHANGER_FAN_OFF_TEMP) {
            exchState = 0;
        }
    }

    if (htrState != -1)
        FIA_SetHeaters(htrState);
    if (blBallState != -1)
        FIA_SetBacklightBallastFans(blBallState);
    if (circState != -1)
        FIA_SetCirculationFans(circState);
    if (exchState != -1)
        FIA_SetHeatExchangerFan(exchState);
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef* hspi) {
    if (hspi == &BITMAP_DATA_SPI) {
        FIA_bitmapRxActive = 0;
        FIA_SetStatusLED(2, 0);
        FIA_SetStatusLED(1, 1);
        if (FIA_bitmapRxBoth) {
            if (FIA_bitmapRxBuf == FIA_staticBufferSideA) {
                memcpy(FIA_staticBufferSideB, FIA_staticBufferSideA, BITMAP_BUF_SIZE);
            } else if (FIA_bitmapRxBuf == FIA_maskBufferSideA) {
                memcpy(FIA_maskBufferSideB, FIA_maskBufferSideA, BITMAP_BUF_SIZE);
            } else if (FIA_bitmapRxBuf == FIA_dynamicBufferSideA) {
                memcpy(FIA_dynamicBufferSideB, FIA_dynamicBufferSideA, BITMAP_BUF_SIZE);
            }
        }
    }
}