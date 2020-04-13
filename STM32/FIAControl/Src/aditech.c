#include "aditech.h"
#include "fia.h"
#include "globals.h"
#include "spi.h"
#include "util.h"

const uint8_t BACKPLANES[NUM_BACKPLANES] = {COM2, COM1, COM3, COM0};

uint8_t LCD_GetEndian(uint8_t backplane) {
    if (backplane == COM0 || backplane == COM1) {
        return MSBFIRST;
    } else {
        return LSBFIRST;
    }
}

uint8_t LCD_GetBackplaneIndex(uint8_t backplane) {
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

uint8_t LCD_GetBitmapByte(uint8_t* data, uint8_t panelRowOffset, uint8_t halfPanelIndex, uint8_t col,
                           uint8_t backplane) {
    // Look up the right byte from the bitmap data buffer specified by the parameters.
    // This assumes the LCD panels are wired in a U-loop such that all top half panels
    // are connected from left to right, then all bottom half panels from right to left.

    uint8_t bplOffset = LCD_GetBackplaneIndex(backplane);
    uint16_t baseIndex;
    uint16_t index;
    uint8_t halfPanelOffset;

    if (halfPanelIndex >= NUM_PANELS) {
        // We're in the top half of the loop.
        // Half panels are indexed from left to right and bitmaps are not rotated.

        // Calculate the internal half panel index (to account for wiring style)
        halfPanelOffset = halfPanelIndex - 5;

        // First, calculate the base offset where the 4 bytes for the indexed column are located
        baseIndex = (halfPanelOffset * PANEL_WIDTH * NUM_PANEL_ROWS * PANEL_HEIGHT / 8) +
                    (col * NUM_PANEL_ROWS * PANEL_HEIGHT / 8) + (panelRowOffset * PANEL_HEIGHT / 8);
        index = baseIndex + bplOffset;
        return data[index];
    } else {
        // We're in the bottom half of the loop.
        // Half panels are indexed from right to left and half panels are rotated 180 degrees.

        // Calculate the internal half panel index (to account for wiring style)
        halfPanelOffset = 4 - halfPanelIndex;

        // First, calculate the base offset where the 4 bytes for the indexed column are located
        // Since half panels are rotated 180 degrees, backplane 0 is at the bottom.
        // Thus, we add the appropriate amount at the end to jump from panel top to panel bottom.
        baseIndex = (halfPanelOffset * PANEL_WIDTH * NUM_PANEL_ROWS * PANEL_HEIGHT / 8) +
                    ((PANEL_WIDTH - (col + 1)) * NUM_PANEL_ROWS * PANEL_HEIGHT / 8) +
                    (panelRowOffset * PANEL_HEIGHT / 8) + (NUM_BACKPLANES * 2) - 1;
        index = baseIndex - bplOffset;
        return reverseByte(data[index]);
    }
}

uint16_t LCD_ConvertBitmap(uint8_t* dst, uint8_t* src, uint8_t panelRowOffset, uint8_t ramSel) {
    // Convert bitmap data to the Aditech LCD format, in half-panel blocks
    // dst: Destination buffer for LCD data. Needs to be 96 bytes per half panel larger than src!
    // src: Source buffer with bitmap data, column major, top byte first
    // panelRowOffset: In case multiple rows of LCD panels are present, specify vertical offset in panel heights
    // ramSel: Which RAM the data should be written to (RAM1 / RAM2)
    // Returns: Length of destination data

    int16_t col;
    uint8_t backplane;
    uint8_t ctrl;
    uint8_t curBitmapByte;
    uint8_t halfByteOffset;
    uint16_t dstAddr = 0;

    // Output format: The 4 backplane data streams, sequentially
    for (uint8_t bplIdx = 0; bplIdx < 4; bplIdx++) {
        backplane = BACKPLANES[bplIdx];

        // Iterate over each half-panel block
        for (uint8_t halfPanelIndex = 0; halfPanelIndex < NUM_HALF_PANELS; halfPanelIndex++) {
            // Iterate over columns
            col = 0;
            while (col < PANEL_WIDTH) {
                if (col == 10 || col == 34 || col == 58 || col == 82) {
                    halfByteOffset = 1;
                } else {
                    halfByteOffset = 0;
                }

                // 5 bytes per control IC
                for (uint8_t i = 0; i < 5; i++) {
                    if (halfByteOffset) {
                        if (LCD_GetEndian(backplane) == LSBFIRST) {
                            curBitmapByte =
                                (reverseByte(
                                     LCD_GetBitmapByte(src, panelRowOffset, halfPanelIndex, col - 1, backplane))
                                 << 4) |
                                (reverseByte(LCD_GetBitmapByte(src, panelRowOffset, halfPanelIndex, col, backplane)) >>
                                 4);
                        } else {
                            curBitmapByte =
                                (LCD_GetBitmapByte(src, panelRowOffset, halfPanelIndex, col - 1, backplane) << 4) |
                                (LCD_GetBitmapByte(src, panelRowOffset, halfPanelIndex, col, backplane) >> 4);
                        }
                    } else {
                        if (LCD_GetEndian(backplane) == LSBFIRST) {
                            curBitmapByte =
                                reverseByte(LCD_GetBitmapByte(src, panelRowOffset, halfPanelIndex, col, backplane));
                        } else {
                            curBitmapByte = LCD_GetBitmapByte(src, panelRowOffset, halfPanelIndex, col, backplane);
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

void LCD_TransmitBackplane(uint8_t* data, uint8_t backplane, uint8_t numHalfPanels, FIA_LCD_Bus_t row) {
    // data: Output from convertBitmap()
    // backplane: The backplane to transmit data for (COM0 ... COM3)
    // numHalfPanels: Number of half-panel blocks to be transmitted

    uint8_t bplIndex = LCD_GetBackplaneIndex(backplane);
    uint16_t dataOffset = bplIndex * BACKPLANE_NUM_LCD_BYTES * numHalfPanels;
    if (row == BUS_1) {
        HAL_SPI_Transmit_DMA(&LCD_SPI_ROW_1, data + dataOffset, BACKPLANE_NUM_LCD_BYTES * numHalfPanels);
    }
    if (row == BUS_2) {
        HAL_SPI_Transmit_DMA(&LCD_SPI_ROW_2, data + dataOffset, BACKPLANE_NUM_LCD_BYTES * numHalfPanels);
    }
    if (row == BUS_3) {
        HAL_SPI_Transmit_DMA(&LCD_SPI_ROW_3, data + dataOffset, BACKPLANE_NUM_LCD_BYTES * numHalfPanels);
    }
    if (row == BUS_4) {
        HAL_SPI_Transmit_DMA(&LCD_SPI_ROW_4, data + dataOffset, BACKPLANE_NUM_LCD_BYTES * numHalfPanels);
    }
}

uint8_t LCD_IsTransmitActive(FIA_LCD_Bus_t row) {
    return transmitActive[row];
}

uint8_t LCD_TransmitBitmap(uint8_t* data, uint8_t numHalfPanels, FIA_LCD_Bus_t row) {
    // data: Output from convertBitmap()
    // numHalfPanels: Number of half-panel blocks to be transmitted

    if (transmitActive[row])
        return 0;
    transmitActive[row] = 1;
    curLCDData[row] = data;
    curTransmittedBackplaneIndex[row] = 0;
    LCD_TransmitBackplane(data, BACKPLANES[curTransmittedBackplaneIndex[row]], NUM_HALF_PANELS, row);
    return 1;
}

void LCD_Latch(FIA_LCD_Bus_t row) {
    if (row == BUS_1) {
        HAL_GPIO_WritePin(LCD1_LATCH_GPIO_Port, LCD1_LATCH_Pin, 1);
        delay_us(1);
        HAL_GPIO_WritePin(LCD1_LATCH_GPIO_Port, LCD1_LATCH_Pin, 0);
    }
    if (row == BUS_2) {
        HAL_GPIO_WritePin(LCD2_LATCH_GPIO_Port, LCD2_LATCH_Pin, 1);
        delay_us(1);
        HAL_GPIO_WritePin(LCD2_LATCH_GPIO_Port, LCD2_LATCH_Pin, 0);
    }
    if (row == BUS_3) {
        HAL_GPIO_WritePin(LCD3_LATCH_GPIO_Port, LCD3_LATCH_Pin, 1);
        delay_us(1);
        HAL_GPIO_WritePin(LCD3_LATCH_GPIO_Port, LCD3_LATCH_Pin, 0);
    }
    if (row == BUS_4) {
        HAL_GPIO_WritePin(LCD4_LATCH_GPIO_Port, LCD4_LATCH_Pin, 1);
        delay_us(1);
        HAL_GPIO_WritePin(LCD4_LATCH_GPIO_Port, LCD4_LATCH_Pin, 0);
    }
}

void LCD_TransmitCompleteCallback(FIA_LCD_Bus_t row) {
    LCD_Latch(row);
    curTransmittedBackplaneIndex[row]++;
    if (curTransmittedBackplaneIndex[row] < NUM_BACKPLANES) {
        LCD_TransmitBackplane(curLCDData[row], BACKPLANES[curTransmittedBackplaneIndex[row]], NUM_HALF_PANELS, row);
    } else {
        curTransmittedBackplaneIndex[row] = 0;
        transmitActive[row] = 0;
    }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi) {
    if (hspi == &LCD_SPI_ROW_1) {
        LCD_TransmitCompleteCallback(BUS_1);
    }
    if (hspi == &LCD_SPI_ROW_2) {
        LCD_TransmitCompleteCallback(BUS_2);
    }
    if (hspi == &LCD_SPI_ROW_3) {
        LCD_TransmitCompleteCallback(BUS_3);
    }
    if (hspi == &LCD_SPI_ROW_4) {
        LCD_TransmitCompleteCallback(BUS_4);
    }
}