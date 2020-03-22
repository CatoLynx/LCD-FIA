#include "sht21.h"
#include "fia.h"
#include "i2c.h"

void SHT21_StartTempConversion(void) {
    uint8_t data[1] = {SHT21_CMD_CONV_TEMP_NO_HOLD};
    HAL_I2C_Master_Transmit(&PERIPHERALS_I2C, I2C_ADDR_SENS_SHT21, data, 1, I2C_TIMEOUT);
}

void SHT21_StartHumidityConversion(void) {
    uint8_t data[1] = {SHT21_CMD_CONV_RH_NO_HOLD};
    HAL_I2C_Master_Transmit(&PERIPHERALS_I2C, I2C_ADDR_SENS_SHT21, data, 1, I2C_TIMEOUT);
}

void SHT21_StartReadout(void) {
    HAL_I2C_Master_Receive_IT(&PERIPHERALS_I2C, I2C_ADDR_SENS_SHT21, sht21RxBuffer, 3);
}

void SHT21_HandleCommunication(void) {
    /*
      So this function exists because for some reason,
      starting the readout right after the (blocking!)
      conversion causes the RxCpltCallback to not fire.
      HAL_I2C_Master_Receive_IT even returns HAL_OK,
      but it just won't work.
      So I'm using this approach of cycling through
      activities on every iteration.
      This means that the sensor updates slower than the rest,
      but that's okay.
    */

    switch (sht21CommState) {
        case READOUT_STARTED_RH:
        case IDLE: {
            SHT21_StartTempConversion();
            sht21CommState = CONV_STARTED_TEMP;
            break;
        }
        case CONV_STARTED_TEMP: {
            SHT21_StartReadout();
            sht21CommState = READOUT_STARTED_TEMP;
            break;
        }
        case READOUT_STARTED_TEMP: {
            SHT21_StartHumidityConversion();
            sht21CommState = CONV_STARTED_RH;
            break;
        }
        case CONV_STARTED_RH: {
            SHT21_StartReadout();
            sht21CommState = READOUT_STARTED_RH;
            break;
        }
    }
}

double SHT21_GetTemperature(void) {
    return sht21CurTemperature;
}

double SHT21_GetHumidity(void) {
    return sht21CurHumidity;
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef* hi2c) {
    if (hi2c != &PERIPHERALS_I2C)
        return;

    if (sht21CommState == READOUT_STARTED_TEMP) {
        uint16_t tempRaw = ((uint16_t)sht21RxBuffer[0] << 8) | sht21RxBuffer[1];
        sht21CurTemperature = (-46.85 + 175.72 * (tempRaw / 65536.0));
    } else if (sht21CommState == READOUT_STARTED_RH) {
        uint16_t rhRaw = ((uint16_t)sht21RxBuffer[0] << 8) | sht21RxBuffer[1];
        sht21CurHumidity = (-6.0 + 125.0 * (rhRaw / 65536.0));
    }
}