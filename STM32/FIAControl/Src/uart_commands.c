#include "uart_commands.h"
#include "fia.h"
#include "uart_protocol.h"
#include "util.h"
#include <string.h>

void UART_ProcessCommand(uint8_t command, uint8_t* parameters, uint8_t parameterLength) {
    uint8_t responseLength = 0;

    switch (command) {
        case UART_CMD_NULL: {
            uartTxPayload[0] = 0xFF;
            responseLength = 1;
            break;
        }

        case UART_CMD_MCU_RESET: {
            NVIC_SystemReset();
            // Will probably not be sent
            uartTxPayload[0] = 0xFF;
            responseLength = 1;
            break;
        }

        case UART_CMD_SET_BACKLIGHT_STATE: {
            FIA_SetBacklight(parameters[0]);
            responseLength = 0;
            break;
        }

        case UART_CMD_GET_BACKLIGHT_STATE: {
            uartTxPayload[0] = FIA_GetBacklight();
            responseLength = 1;
            break;
        }

        case UART_CMD_SET_BACKLIGHT_BASE_BRIGHTNESS: {
            int16_t brtA = ((int16_t)parameters[0] << 8) | parameters[1];
            int16_t brtB = ((int16_t)parameters[2] << 8) | parameters[3];
            FIA_SetBacklightBaseBrightness(SIDE_A, brtA);
            FIA_SetBacklightBaseBrightness(SIDE_B, brtB);
            responseLength = 0;
            break;
        }

        case UART_CMD_GET_BACKLIGHT_BASE_BRIGHTNESS: {
            int16_t brtA = FIA_GetBacklightBaseBrightness(SIDE_A);
            int16_t brtB = FIA_GetBacklightBaseBrightness(SIDE_B);
            uartTxPayload[0] = brtA >> 8;
            uartTxPayload[1] = brtA & 0xFF;
            uartTxPayload[2] = brtB >> 8;
            uartTxPayload[3] = brtB & 0xFF;
            responseLength = 4;
            break;
        }

        case UART_CMD_GET_BACKLIGHT_BRIGHTNESS: {
            uint16_t brtA = FIA_GetBacklightBrightness(SIDE_A);
            uint16_t brtB = FIA_GetBacklightBrightness(SIDE_B);
            uartTxPayload[0] = brtA >> 8;
            uartTxPayload[1] = brtA & 0xFF;
            uartTxPayload[2] = brtB >> 8;
            uartTxPayload[3] = brtB & 0xFF;
            responseLength = 4;
            break;
        }

        case UART_CMD_SET_HEATERS_STATE: {
            FIA_SetHeaters(parameters[0]);
            responseLength = 0;
            break;
        }

        case UART_CMD_GET_HEATERS_STATE: {
            uartTxPayload[0] = FIA_GetHeaters();
            responseLength = 1;
            break;
        }

        case UART_CMD_SET_CIRCULATION_FANS_STATE: {
            FIA_SetCirculationFans(parameters[0]);
            responseLength = 0;
            break;
        }

        case UART_CMD_GET_CIRCULATION_FANS_STATE: {
            uartTxPayload[0] = FIA_GetCirculationFans();
            responseLength = 1;
            break;
        }

        case UART_CMD_SET_HEAT_EXCHANGER_FAN_STATE: {
            FIA_SetHeatExchangerFan(parameters[0]);
            responseLength = 0;
            break;
        }

        case UART_CMD_GET_HEAT_EXCHANGER_FAN_STATE: {
            uartTxPayload[0] = FIA_GetHeatExchangerFan();
            responseLength = 1;
            break;
        }

        case UART_CMD_SET_BACKLIGHT_BALLAST_FANS_STATE: {
            FIA_SetBacklightBallastFans(parameters[0]);
            responseLength = 0;
            break;
        }

        case UART_CMD_GET_BACKLIGHT_BALLAST_FANS_STATE: {
            uartTxPayload[0] = FIA_GetBacklightBallastFans();
            responseLength = 1;
            break;
        }

        case UART_CMD_GET_DOOR_STATES: {
            uartTxPayload[0] = FIA_GetDoors();
            responseLength = 1;
            break;
        }

        case UART_CMD_GET_TEMPERATURES: {
            int16_t tempExt1 = FIA_GetTemperature(BL_BALL) * 100;
            int16_t tempExt2 = FIA_GetTemperature(AIRFLOW) * 100;
            int16_t tempBoard = FIA_GetTemperature(BOARD) * 100;
            int16_t tempMCU = FIA_GetTemperature(MCU) * 100;
            uartTxPayload[0] = tempExt1 >> 8;
            uartTxPayload[1] = tempExt1 & 0xFF;
            uartTxPayload[2] = tempExt2 >> 8;
            uartTxPayload[3] = tempExt2 & 0xFF;
            uartTxPayload[4] = tempBoard >> 8;
            uartTxPayload[5] = tempBoard & 0xFF;
            uartTxPayload[6] = tempMCU >> 8;
            uartTxPayload[7] = tempMCU & 0xFF;
            responseLength = 8;
            break;
        }

        case UART_CMD_GET_HUMIDITY: {
            int16_t humidity = FIA_GetHumidity() * 100;
            uartTxPayload[0] = humidity >> 8;
            uartTxPayload[1] = humidity & 0xFF;
            responseLength = 2;
            break;
        }

        case UART_CMD_GET_ENV_BRIGHTNESS: {
            uint16_t brtA = FIA_GetEnvBrightness(SIDE_A);
            uint16_t brtB = FIA_GetEnvBrightness(SIDE_B);
            uartTxPayload[0] = brtA >> 8;
            uartTxPayload[1] = brtA & 0xFF;
            uartTxPayload[2] = brtB >> 8;
            uartTxPayload[3] = brtB & 0xFF;
            responseLength = 4;
            break;
        }

        case UART_CMD_SET_LCD_CONTRAST: {
            uint16_t ctrA = ((uint16_t)parameters[0] << 8) | parameters[1];
            uint16_t ctrB = ((uint16_t)parameters[2] << 8) | parameters[3];
            FIA_SetLCDContrast(SIDE_A, ctrA);
            FIA_SetLCDContrast(SIDE_B, ctrB);
            responseLength = 0;
            break;
        }

        case UART_CMD_GET_LCD_CONTRAST: {
            uint16_t ctrA = FIA_GetLCDContrast(SIDE_A);
            uint16_t ctrB = FIA_GetLCDContrast(SIDE_B);
            uartTxPayload[0] = ctrA >> 8;
            uartTxPayload[1] = ctrA & 0xFF;
            uartTxPayload[2] = ctrB >> 8;
            uartTxPayload[3] = ctrB & 0xFF;
            responseLength = 4;
            break;
        }

        case UART_CMD_CREATE_SCROLL_BUFFER: {
            responseLength = 1;

            FIA_Side_t side = parameters[0];
            uint16_t dispX = ((uint16_t)parameters[1] << 8) | parameters[2];
            uint16_t dispY = ((uint16_t)parameters[3] << 8) | parameters[4];
            uint16_t dispW = ((uint16_t)parameters[5] << 8) | parameters[6];
            uint16_t dispH = ((uint16_t)parameters[7] << 8) | parameters[8];
            uint16_t intW = ((uint16_t)parameters[9] << 8) | parameters[10];
            uint16_t intH = ((uint16_t)parameters[11] << 8) | parameters[12];
            uint8_t id = FIA_CreateScrollBuffer(side, dispX, dispY, dispW, dispH, intW, intH);

            uartTxPayload[0] = id;
            break;
        }

        case UART_CMD_DELETE_SCROLL_BUFFER: {
            responseLength = 1;
            uint8_t status = FIA_DeleteScrollBuffer(parameters[0]);
            uartTxPayload[0] = status;
            break;
        }

        case UART_CMD_SET_DESTINATION_BUFFER: {
            responseLength = 1;
            uint8_t status = FIA_SetBitmapDestinationBuffer(parameters[0]);
            uartTxPayload[0] = status;
            break;
        }
    }

    UART_TransmitResponse(uartTxPayload, responseLength);
}