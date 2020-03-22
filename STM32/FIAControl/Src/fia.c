#include "fia.h"
#include "ds18b20.h"
#include "sht21.h"
#include "stm32f4xx_hal.h"
#include "util.h"

uint16_t FIA_BacklightBaseBrightness[2] = {2048, 2048};
uint8_t firstADCReadFlag = 1;

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

    FIA_SetBacklight(0);
    FIA_SetHeaters(0);
    FIA_SetCirculationFans(0);
    FIA_SetHeatExchangerFan(0);
    FIA_SetBacklightBallastFans(0);

    FIA_SetStatusLED(1, 0);
    FIA_SetStatusLED(2, 0);
}

void FIA_InitI2CDACs(void) {
    uint8_t buf[3];
    buf[0] = 0b01000000; // Cmd: Write volatile mem, conf: ref=VDD, no power down, gain=1
    buf[1] = 0x00;
    buf[2] = 0x00;
    HAL_I2C_Master_Transmit(&PERIPHERALS_I2C, I2C_ADDR_DAC_BRIGHTNESS_A, buf, 3, I2C_TIMEOUT);
    HAL_I2C_Master_Transmit(&PERIPHERALS_I2C, I2C_ADDR_DAC_BRIGHTNESS_B, buf, 3, I2C_TIMEOUT);
}

void FIA_SetBacklightBrightness(FIA_Side_t side, uint16_t value) {
    uint8_t buf[2];
    buf[0] = (value >> 8) & 0x0F;
    buf[1] = value & 0xFF;
    switch (side) {
        case SIDE_A: {
            HAL_I2C_Master_Transmit(&PERIPHERALS_I2C, I2C_ADDR_DAC_BRIGHTNESS_A, buf, 2, I2C_TIMEOUT);
            break;
        }
        case SIDE_B: {
            HAL_I2C_Master_Transmit(&PERIPHERALS_I2C, I2C_ADDR_DAC_BRIGHTNESS_B, buf, 2, I2C_TIMEOUT);
            break;
        }
        case SIDE_BOTH: {
            HAL_I2C_Master_Transmit(&PERIPHERALS_I2C, I2C_ADDR_DAC_BRIGHTNESS_A, buf, 2, I2C_TIMEOUT);
            HAL_I2C_Master_Transmit(&PERIPHERALS_I2C, I2C_ADDR_DAC_BRIGHTNESS_B, buf, 2, I2C_TIMEOUT);
            break;
        }
        case SIDE_NONE: {
            break;
        }
    }
}

void FIA_SetLCDContrast(FIA_Side_t side, uint16_t value) {
    switch (side) {
        case SIDE_A: {
            HAL_DAC_SetValue(&hdac, DAC_LCD_CONTRAST_SIDE_A, DAC_ALIGN_12B_R, value);
            break;
        }
        case SIDE_B: {
            HAL_DAC_SetValue(&hdac, DAC_LCD_CONTRAST_SIDE_B, DAC_ALIGN_12B_R, value);
            break;
        }
        case SIDE_BOTH: {
            HAL_DAC_SetValue(&hdac, DAC_LCD_CONTRAST_SIDE_A, DAC_ALIGN_12B_R, value);
            HAL_DAC_SetValue(&hdac, DAC_LCD_CONTRAST_SIDE_B, DAC_ALIGN_12B_R, value);
            break;
        }
        case SIDE_NONE: {
            break;
        }
    }
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

void FIA_UpdateADCValues() {
    // CH1, CH2 (Env Brightness) sensor range: 0.3 ... 0.65V (ADC values 370 ... 800)
    // CH3 (Internal Temperature) sensor range: ???
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
        envBrightness[c] = (uint32_t)limitRange(
            mapRange(adcAverages[c], 370, 800, 0, 4095) + FIA_BacklightBaseBrightness[c] - 2048, 0, 4095);
    }

    // ...and the internal temperature sensor
    tempSensorValues[3] = ((mapRange(adcAverages[2], 0, 4095, 0, 3.3) - TEMP_SENS_V25) / TEMP_SENS_AVG_SLOPE) + 25.0;

    updateBacklightBrightnessFlag = 1;
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