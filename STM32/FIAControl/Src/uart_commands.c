#include "uart_commands.h"
#include "fia.h"
#include "uart_protocol.h"
#include "util.h"
#include <string.h>

extern uint8_t uartTxPayload[UART_MAX_PAYLOAD_LENGTH - 1];


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
            uint16_t scOffX = ((uint16_t)parameters[13] << 8) | parameters[14];
            uint16_t scOffY = ((uint16_t)parameters[15] << 8) | parameters[16];
            uint16_t scSpX = ((uint16_t)parameters[17] << 8) | parameters[18];
            uint16_t scSpY = ((uint16_t)parameters[19] << 8) | parameters[20];
            int16_t scStX = ((int16_t)parameters[21] << 8) | parameters[22];
            int16_t scStY = ((int16_t)parameters[23] << 8) | parameters[24];
            uint8_t id = FIA_CreateScrollBuffer(side, dispX, dispY, dispW, dispH, intW, intH, scOffX, scOffY, scSpX,
                                                scSpY, scStX, scStY);

            uartTxPayload[0] = id;
            break;
        }

        case UART_CMD_DELETE_SCROLL_BUFFER: {
            responseLength = 1;
            uint8_t status = FIA_DeleteScrollBuffer(parameters[0]);
            uartTxPayload[0] = status;
            break;
        }

        case UART_CMD_UPDATE_SCROLL_BUFFER: {
            responseLength = 1;

            uint8_t id = parameters[0];
            FIA_Side_t side = parameters[1];
            uint16_t dispX = ((uint16_t)parameters[2] << 8) | parameters[3];
            uint16_t dispY = ((uint16_t)parameters[4] << 8) | parameters[5];
            uint16_t dispW = ((uint16_t)parameters[6] << 8) | parameters[7];
            uint16_t dispH = ((uint16_t)parameters[8] << 8) | parameters[9];
            uint16_t scOffX = ((uint16_t)parameters[10] << 8) | parameters[11];
            uint16_t scOffY = ((uint16_t)parameters[12] << 8) | parameters[13];
            uint16_t scSpX = ((uint16_t)parameters[14] << 8) | parameters[15];
            uint16_t scSpY = ((uint16_t)parameters[16] << 8) | parameters[17];
            int16_t scStX = ((int16_t)parameters[18] << 8) | parameters[19];
            int16_t scStY = ((int16_t)parameters[20] << 8) | parameters[21];
            uint8_t status = FIA_UpdateScrollBuffer(id, side, dispX, dispY, dispW, dispH, scOffX, scOffY, scSpX,
                                                scSpY, scStX, scStY);

            uartTxPayload[0] = status;
            break;
        }

        case UART_CMD_SET_DESTINATION_BUFFER: {
            responseLength = 1;
            uint8_t status = FIA_SetBitmapDestinationBuffer(parameters[0]);
            uartTxPayload[0] = status;
            break;
        }

        case UART_CMD_GET_DESTINATION_BUFFER: {
            responseLength = 1;
            uartTxPayload[0] = FIA_GetBitmapDestinationBuffer();
            break;
        }

        case UART_CMD_SET_MASK_ENABLED: {
            FIA_SetMaskEnabled(parameters[0]);
            responseLength = 0;
            break;
        }

        case UART_CMD_GET_MASK_ENABLED: {
            responseLength = 1;
            uartTxPayload[0] = FIA_GetMaskEnabled();
            break;
        }
    }

    UART_TransmitResponse(uartTxPayload, responseLength);
}