#include "fia.h"
#include "util.h"
#include "stm32f4xx_hal.h"

uint32_t baseBrightness[2] = {2048, 2048};
uint8_t firstADCReadFlag = 1;

void FIA_Init() {
    HAL_TIM_Base_Start(&DELAY_US_TIMER);
    HAL_TIM_Base_Start(&LCD_FRAME_TIMER);
    HAL_TIM_Base_Start(&ADC_UPDATE_TIMER);
    HAL_TIM_Base_Start_IT(&ADC_UPDATE_TIMER);
    HAL_TIM_PWM_Start(&LCD_FRAME_TIMER, TIM_CHANNEL_1);
    HAL_DAC_Start(&LCD_CONTRAST_DAC, DAC1_CHANNEL_1);
    HAL_DAC_Start(&LCD_CONTRAST_DAC, DAC1_CHANNEL_2);
    HAL_ADC_Start_DMA(&ENV_BRIGHTNESS_ADC, adcValues, ADC_NUM_CHANNELS);
    FIA_InitI2CDACs();
    FIA_setStatusLED(1, 0);
    FIA_setStatusLED(2, 0);
}

void FIA_InitI2CDACs() {
    uint8_t buf[3];
    buf[0] = 0b01000000; // Cmd: Write volatile mem, conf: ref=VDD, no power down, gain=1
    buf[1] = 0x00;
    buf[2] = 0x00;
    HAL_I2C_Master_Transmit(&PERIPHERALS_I2C, I2C_ADDR_DAC_BRIGHTNESS_A, buf, 3, I2C_TIMEOUT);
    HAL_I2C_Master_Transmit(&PERIPHERALS_I2C, I2C_ADDR_DAC_BRIGHTNESS_B, buf, 3, I2C_TIMEOUT);
}

void FIA_setBacklightBrightness(FIA_Side_t side, uint16_t value) {
    uint8_t buf[2];
    buf[0] = (value >> 8) & 0x0F;
    buf[1] = value & 0xFF;
    HAL_I2C_Master_Transmit(&PERIPHERALS_I2C, side == SIDE_A ? I2C_ADDR_DAC_BRIGHTNESS_A : I2C_ADDR_DAC_BRIGHTNESS_B,
                            buf, 2, I2C_TIMEOUT);
}

void FIA_setLCDContrast(FIA_Side_t side, uint16_t value) {
    HAL_DAC_SetValue(&hdac, side == SIDE_A ? DAC_LCD_CONTRAST_SIDE_A : DAC_LCD_CONTRAST_SIDE_B, DAC_ALIGN_12B_R, value);
}

void FIA_setStatusLED(uint8_t number, uint8_t value) {
    if (number == 1) {
        HAL_GPIO_WritePin(STATUS_LED_1_GPIO_Port, STATUS_LED_1_Pin, !value);
    } else if (number == 2) {
        HAL_GPIO_WritePin(STATUS_LED_2_GPIO_Port, STATUS_LED_2_Pin, !value);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if(firstADCReadFlag) {
        firstADCReadFlag = 0;
        firstADCAverageFlag = 1;
    }
}

uint16_t FIA_getEnvBrightness(FIA_Side_t side) {
    return envBrightness[side];
}

void FIA_updateEnvBrightness() {
    // Sensor range: 0.3 ... 0.65V (ADC values 370 ... 800)
    for(uint8_t c = 0; c < ADC_NUM_CHANNELS; c++) {
        adcRingBuffer[adcRingBufferIndex++] = adcValues[c];
    }
    if(adcRingBufferIndex >= (ADC_NUM_CHANNELS * ADC_AVG_COUNT)) adcRingBufferIndex = 0;
    if(firstADCAverageFlag) {
        for(uint8_t c = 0; c < ADC_NUM_CHANNELS; c++) {
            for(uint32_t i = c; i < ADC_AVG_COUNT * ADC_NUM_CHANNELS; i += ADC_NUM_CHANNELS) {
                adcRingBuffer[i] = adcValues[c];
            }
        }
        firstADCAverageFlag = 0;
    }
    averageADCValues(adcRingBuffer, adcAverages, ADC_NUM_CHANNELS, ADC_AVG_COUNT);
    for(uint8_t c = 0; c < ADC_NUM_CHANNELS; c++) {
	    envBrightness[c] = (uint32_t)limitRange(mapRange(adcAverages[c], 370, 800, 0, 4095) - baseBrightness[c] + 2048, 0, 4095);
    }
    updateBacklightBrightnessFlag = 1;
}