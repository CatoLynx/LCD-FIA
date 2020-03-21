#include "aditech.h"
#include "fia.h"
#include "globals.h"
#include "spi.h"
#include "util.h"

const uint8_t BACKPLANES[NUM_BACKPLANES] = {COM2, COM1, COM3, COM0};

uint8_t getEndian(uint8_t backplane) {
    if (backplane == COM0 || backplane == COM1) {
        return MSBFIRST;
    } else {
        return LSBFIRST;
    }
}

uint8_t getBackplaneIndex(uint8_t backplane) {
    switch (backplane) {
        case COM0: {
            return 3;
        }
        case COM1: {
            return 1;
        }
        case COM2: {
            return 0;
        }
        case COM3: {
            return 2;
        }
    }
    return 0;
}

uint8_t getBitmapByte(uint8_t* data, uint8_t col, uint8_t backplane) {
    uint8_t offset = getBackplaneIndex(backplane);
    return data[col * NUM_BACKPLANES + offset];
}

uint8_t reverseByte(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

uint16_t convertBitmap(uint8_t* dst, uint8_t* src, uint8_t numHalfPanels, uint8_t ramSel) {
    // Convert "normal" bitmap data to the Aditech LCD format, in half-panel blocks
    // dst: Destination buffer for LCD data. Needs to be 96 bytes per half panel larger than src!
    // src: Source buffer with bitmap data
    // numHalfPanels: Number of half-panel blocks in the source data
    // ramSel: Which RAM the data should be written to (RAM1 / RAM2)
    // Returns: Length of destination data

    int16_t col;
    uint8_t backplane;
    uint8_t ctrl;
    uint8_t curBitmapByte;
    uint8_t halfByteOffset;
    uint16_t srcOffset;
    uint16_t dstAddr = 0;

    // Output format: The 4 backplane data streams, sequentially
    for (uint8_t bplIdx = 0; bplIdx < 4; bplIdx++) {
        backplane = BACKPLANES[bplIdx];

        // Iterate over each half-panel block
        for (uint8_t halfPanelIndex = 0; halfPanelIndex < numHalfPanels; halfPanelIndex++) {
            srcOffset = halfPanelIndex * HALF_PANEL_NUM_BITMAP_BYTES;

            // Iterate over columns
            col = 0;
            while (col <= PANEL_WIDTH - 1) {
                if (col == 10 || col == 34 || col == 58 || col == 82) {
                    halfByteOffset = 1;
                } else {
                    halfByteOffset = 0;
                }

                // 5 bytes per control IC
                for (uint8_t i = 0; i < 5; i++) {
                    if (halfByteOffset) {
                        if (getEndian(backplane) == LSBFIRST) {
                            curBitmapByte = (reverseByte(getBitmapByte(src + srcOffset, col - 1, backplane)) << 4) |
                                            (reverseByte(getBitmapByte(src + srcOffset, col, backplane)) >> 4);
                        } else {
                            curBitmapByte = (getBitmapByte(src + srcOffset, col - 1, backplane) << 4) |
                                            (getBitmapByte(src + srcOffset, col, backplane) >> 4);
                        }
                    } else {
                        if (getEndian(backplane) == LSBFIRST) {
                            curBitmapByte = reverseByte(getBitmapByte(src + srcOffset, col, backplane));
                        } else {
                            curBitmapByte = getBitmapByte(src + srcOffset, col, backplane);
                        }
                    }

                    dst[dstAddr++] = curBitmapByte;

                    if ((halfByteOffset && i < 4) || !halfByteOffset) {
                        col++;
                    }
                }

                // Control byte
                ctrl = MUX4 | ramSel | backplane;
                dst[dstAddr++] = ctrl;
            }
        }
    }
    return dstAddr;
}

void transmitBackplane(uint8_t* data, uint8_t backplane, uint8_t numHalfPanels, FIA_LCD_Row_t row) {
    // data: Output from convertBitmap()
    // backplane: The backplane to transmit data for (COM0 ... COM3)
    // numHalfPanels: Number of half-panel blocks to be transmitted

    uint8_t bplIndex = getBackplaneIndex(backplane);
    uint16_t dataOffset = bplIndex * BACKPLANE_NUM_LCD_BYTES * numHalfPanels;
    if (row == ROW_1) {
        HAL_SPI_Transmit_DMA(&LCD_SPI_ROW_1, data + dataOffset, BACKPLANE_NUM_LCD_BYTES * numHalfPanels);
    }
    if (row == ROW_2) {
        HAL_SPI_Transmit_DMA(&LCD_SPI_ROW_2, data + dataOffset, BACKPLANE_NUM_LCD_BYTES * numHalfPanels);
    }
    if (row == ROW_3) {
        HAL_SPI_Transmit_DMA(&LCD_SPI_ROW_3, data + dataOffset, BACKPLANE_NUM_LCD_BYTES * numHalfPanels);
    }
    if (row == ROW_4) {
        HAL_SPI_Transmit_DMA(&LCD_SPI_ROW_4, data + dataOffset, BACKPLANE_NUM_LCD_BYTES * numHalfPanels);
    }
}

uint8_t isTransmitActive(FIA_LCD_Row_t row) {
    return transmitActive[row];
}

uint8_t transmitBitmap(uint8_t* data, uint8_t numHalfPanels, FIA_LCD_Row_t row) {
    // data: Output from convertBitmap()
    // numHalfPanels: Number of half-panel blocks to be transmitted

    if (transmitActive[row])
        return 0;
    transmitActive[row] = 1;
    curLCDData[row] = data;
    curTransmittedBackplaneIndex[row] = 0;
    transmitBackplane(data, BACKPLANES[curTransmittedBackplaneIndex[row]], NUM_HALF_PANELS, row);
    return 1;
}

void LCDLatch(FIA_LCD_Row_t row) {
    if (row == ROW_1) {
        HAL_GPIO_WritePin(LCD_LATCH_ROW_1, 1);
        delay_us(1);
        HAL_GPIO_WritePin(LCD_LATCH_ROW_1, 0);
    }
    if (row == ROW_2) {
        HAL_GPIO_WritePin(LCD_LATCH_ROW_2, 1);
        delay_us(1);
        HAL_GPIO_WritePin(LCD_LATCH_ROW_2, 0);
    }
    if (row == ROW_3) {
        HAL_GPIO_WritePin(LCD_LATCH_ROW_3, 1);
        delay_us(1);
        HAL_GPIO_WritePin(LCD_LATCH_ROW_3, 0);
    }
    if (row == ROW_4) {
        HAL_GPIO_WritePin(LCD_LATCH_ROW_4, 1);
        delay_us(1);
        HAL_GPIO_WritePin(LCD_LATCH_ROW_4, 0);
    }
}

void LCDTransmitCompleteCallback(FIA_LCD_Row_t row) {
    LCDLatch(row);
    curTransmittedBackplaneIndex[row]++;
    if (curTransmittedBackplaneIndex[row] < NUM_BACKPLANES) {
        transmitBackplane(curLCDData[row], BACKPLANES[curTransmittedBackplaneIndex[row]], NUM_HALF_PANELS, row);
    } else {
        curTransmittedBackplaneIndex[row] = 0;
        transmitActive[row] = 0;
    }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi) {
    if (hspi == &LCD_SPI_ROW_1) {
        LCDTransmitCompleteCallback(ROW_1);
    }
    if (hspi == &LCD_SPI_ROW_2) {
        LCDTransmitCompleteCallback(ROW_2);
    }
    if (hspi == &LCD_SPI_ROW_3) {
        LCDTransmitCompleteCallback(ROW_3);
    }
    if (hspi == &LCD_SPI_ROW_4) {
        LCDTransmitCompleteCallback(ROW_4);
    }
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef* hspi) {
    if (hspi == &BITMAP_DATA_SPI) {
        bitmapReceiveActive = 0;
        FIA_setStatusLED(2, 0);
    }
}