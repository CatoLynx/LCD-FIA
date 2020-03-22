#pragma once

/*
  uart_commands.h / uart_commands.c - List of commands and parameters
  for the UART protocol as well as the command handler logic
*/

#include <stdint.h>

// Commands
#define UART_CMD_NULL 0x00

#define UART_CMD_SET_BACKLIGHT_STATE 0x10
#define UART_CMD_GET_BACKLIGHT_STATE 0x11
#define UART_CMD_SET_BACKLIGHT_BASE_BRIGHTNESS 0x12
#define UART_CMD_GET_BACKLIGHT_BASE_BRIGHTNESS 0x13

#define UART_CMD_SET_HEATERS_STATE 0x20
#define UART_CMD_GET_HEATERS_STATE 0x21
#define UART_CMD_SET_CIRCULATION_FANS_STATE 0x22
#define UART_CMD_GET_CIRCULATION_FANS_STATE 0x23
#define UART_CMD_SET_HEAT_EXCHANGER_FAN_STATE 0x24
#define UART_CMD_GET_HEAT_EXCHANGER_FAN_STATE 0x25
#define UART_CMD_SET_BACKLIGHT_BALLAST_FANS_STATE 0x26
#define UART_CMD_GET_BACKLIGHT_BALLAST_FANS_STATE 0x27

#define UART_CMD_GET_DOOR_STATES 0x30

#define UART_CMD_GET_TEMPERATURES 0x40
#define UART_CMD_GET_HUMIDITY 0x41

// Function prototypes
void UART_ProcessCommand(uint8_t command, uint8_t* parameters, uint8_t parameterLength);