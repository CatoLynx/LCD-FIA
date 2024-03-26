#pragma once

/*
  uart_protocol.h / uart_protocol.c - Library for the serial protocol

  This protocol is used to communicate non-bitmap data
  between the high-level controller and the STM32 MCU.
*/

#include "uart_commands.h"
#include "usart.h"
#include <stdint.h>

// Global definitions
#define UART_START_BYTE 0xFF
#define UART_CHECKSUM_START_VALUE 0x7F
#define UART_MIN_COMMAND_LENGTH 4 // Start byte, length byte, command byte, checksum
#define UART_MAX_PAYLOAD_LENGTH 27 // Protocol max is 255, but 25 is the parameter length for the longest command + 2 for command and checksum bytes

#define UART_RX_RING_BUFFER_SIZE 256
#define UART_DMA_WRITE_PTR ((UART_RX_RING_BUFFER_SIZE - CONTROL_UART.hdmarx->Instance->NDTR) & (UART_RX_RING_BUFFER_SIZE - 1))

// Function prototypes
void UART_StartRxRingBuffer(void);
uint8_t UART_RxRingBufferEmpty(void);
uint8_t UART_RxRingBufferWaiting(void);
uint8_t UART_RxRingBufferRead(void);
uint8_t UART_RxRingBufferPeek(uint8_t offset);
uint8_t UART_CalculateChecksum(uint8_t* data, uint8_t length);
uint8_t UART_VerifyChecksum(uint8_t* data, uint8_t length);
void UART_TransmitResponse(uint8_t* data, uint8_t length);
void UART_HandleProtocol(void);