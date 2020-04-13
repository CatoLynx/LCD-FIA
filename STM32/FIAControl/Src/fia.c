#include "fia.h"
#include "aditech.h"
#include "ds18b20.h"
#include "sht21.h"
#include "spi.h"
#include "stm32f4xx_hal.h"
#include "uart_protocol.h"
#include "util.h"
#include <string.h>

extern DMA_HandleTypeDef hdma_spi5_rx;

int16_t backlightBaseBrightness[2] = {2048, 2048};
uint16_t lcdContrast[2] = {2048, 2048};
uint8_t firstADCReadFlag = 1;

// LCD Data buffers per LCD bus
uint8_t lcdData1[LCD_BUF_SIZE];
uint8_t lcdData2[LCD_BUF_SIZE];
uint8_t lcdData3[LCD_BUF_SIZE];
uint8_t lcdData4[LCD_BUF_SIZE];

// Status variables for selected RAM per LCD bus
uint8_t currentRAM[NUM_LCD_BUSES];

FIA_Side_t oldDoorStatus = SIDE_NONE;

void FIA_Init(void) {
    HAL_TIM_Base_Start(&DELAY_US_TIMER);
    HAL_TIM_Base_Start(&LCD_FRAME_TIMER);
    HAL_TIM_PWM_Start(&LCD_FRAME_TIMER, TIM_CHANNEL_1);
    HAL_TIM_Base_Start(&ADC_UPDATE_TIMER);
    HAL_TIM_Base_Start_IT(&ADC_UPDATE_TIMER);
    HAL_TIM_Base_Start(&DS18B20_UPDATE_TIMER);
    HAL_TIM_Base_Start_IT(&DS18B20_UPDATE_TIMER);
    HAL_DAC_Start(&LCD_CONTRAST_DAC, DAC1_CHANNEL_1);
    HAL_DAC_Start(&LCD_CONTRAST_DAC, DAC1_CHANNEL_2);
    HAL_ADC_Start_DMA(&ENV_BRIGHTNESS_ADC, adcValues, ADC_NUM_CHANNELS);

    FIA_InitI2CDACs();

    FIA_SetStatusLED(1, 0);
    FIA_SetStatusLED(2, 0);

    UART_StartRxRingBuffer();

    memset(bitmapBufferSideA, 0xFF, BITMAP_BUF_SIZE);
    memset(bitmapBufferSideB, 0xFF, BITMAP_BUF_SIZE);

    FIA_SetLCDContrast(SIDE_A, 2200);
    FIA_SetLCDContrast(SIDE_B, 2200);

    FIA_SetBacklightBrightness(SIDE_A, 0);
    FIA_SetBacklightBrightness(SIDE_B, 0);

    FIA_SetHeaters(0);
    FIA_SetBacklight(1);
    HAL_Delay(500);
    FIA_SetBacklightBallastFans(1);
    HAL_Delay(500);
    FIA_SetCirculationFans(1);
    HAL_Delay(500);
    FIA_SetCirculationFans(2);
    HAL_Delay(500);
    FIA_SetHeatExchangerFan(1);
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

    if (!bitmapReceiveActive) {
        FIA_ReceiveBitmapData();
    }

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

    if (!LCD_IsTransmitActive(BUS_1)) {
        LCD_ConvertBitmap(lcdData1, bitmapBufferSideA, 0, currentRAM[BUS_1]);
        LCD_TransmitBitmap(lcdData1, NUM_HALF_PANELS, BUS_1);
        if (currentRAM[BUS_1] == RAM1) {
            currentRAM[BUS_1] = RAM2;
        } else {
            currentRAM[BUS_1] = RAM1;
        }
    }
    if (!LCD_IsTransmitActive(BUS_2)) {
        LCD_ConvertBitmap(lcdData2, bitmapBufferSideA, 1, currentRAM[BUS_2]);
        LCD_TransmitBitmap(lcdData2, NUM_HALF_PANELS, BUS_2);
        if (currentRAM[BUS_2] == RAM1) {
            currentRAM[BUS_2] = RAM2;
        } else {
            currentRAM[BUS_2] = RAM1;
        }
    }
    if (!LCD_IsTransmitActive(BUS_3)) {
        LCD_ConvertBitmap(lcdData3, bitmapBufferSideA, 0, currentRAM[BUS_3]);
        LCD_TransmitBitmap(lcdData3, NUM_HALF_PANELS, BUS_3);
        if (currentRAM[BUS_3] == RAM1) {
            currentRAM[BUS_3] = RAM2;
        } else {
            currentRAM[BUS_3] = RAM1;
        }
    }
    if (!LCD_IsTransmitActive(BUS_4)) {
        LCD_ConvertBitmap(lcdData4, bitmapBufferSideA, 1, currentRAM[BUS_4]);
        LCD_TransmitBitmap(lcdData4, NUM_HALF_PANELS, BUS_4);
        if (currentRAM[BUS_4] == RAM1) {
            currentRAM[BUS_4] = RAM2;
        } else {
            currentRAM[BUS_4] = RAM1;
        }
    }
}

void FIA_ReceiveBitmapData(void) {
    HAL_SPI_Receive_DMA(&BITMAP_DATA_SPI, bitmapBufferSideA, BITMAP_BUF_SIZE);
    FIA_SetStatusLED(2, 1);
    bitmapReceiveActive = 1;
}

void FIA_AbortBitmapReceive(void) {
    // TODO: Fix abort on CS deassert
    if (1 || !bitmapReceiveActive)
        return;
    HAL_DMA_Abort(&hdma_spi5_rx);
    FIA_SetStatusLED(2, 0);
    bitmapReceiveActive = 0;
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

void FIA_UpdateLCDContrast() {
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

void FIA_UpdateADCValues() {
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
    tempSensorValues[3] = ((mapRange(adcAverages[2], 0, 4095, 0, 3.3) - TEMP_SENS_V25) / TEMP_SENS_AVG_SLOPE) + 25.0;
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
    tempSensorValues[0] = DS18B20_ReadTemperature(TEMP_1_ONEWIRE_GPIO_Port, TEMP_1_ONEWIRE_Pin);
    tempSensorValues[1] = DS18B20_ReadTemperature(TEMP_2_ONEWIRE_GPIO_Port, TEMP_2_ONEWIRE_Pin);
    tempSensorValues[2] = SHT21_GetTemperature();
    SHT21_HandleCommunication();
}

double FIA_GetTemperature(FIA_Temp_Sensor_t sensor) {
    // Get the value for the specified temperature sensor
    switch (sensor) {
        case EXT_1: {
            return tempSensorValues[0];
        }
        case EXT_2: {
            return tempSensorValues[1];
        }
        case BOARD: {
            return tempSensorValues[2];
        }
        case MCU: {
            return tempSensorValues[3];
        }
    }
    return 0;
}

double FIA_GetHumidity() {
    // Get the relative humidity of the inside air (in %)
    return SHT21_GetHumidity();
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef* hspi) {
    if (hspi == &BITMAP_DATA_SPI) {
        bitmapReceiveActive = 0;
        FIA_SetStatusLED(2, 0);
    }
}