#include "uart_protocol.h"
#include "fia.h"
#include <string.h>

void UART_StartRxRingBuffer(void) {
    HAL_UART_Receive_DMA(&CONTROL_UART, uartRxRingBuffer, UART_RX_RING_BUFFER_SIZE);
    uartRxRingBufferReadPtr = 0;
}

uint8_t UART_RxRingBufferEmpty(void) {
    return (uartRxRingBufferReadPtr == UART_DMA_WRITE_PTR);
}

uint8_t UART_RxRingBufferWaiting(void) {
    if (UART_DMA_WRITE_PTR >= uartRxRingBufferReadPtr) {
        return UART_DMA_WRITE_PTR - uartRxRingBufferReadPtr;
    } else {
        return (UART_DMA_WRITE_PTR + UART_RX_RING_BUFFER_SIZE) - uartRxRingBufferReadPtr;
    }
}

uint8_t UART_RxRingBufferRead(void) {
    uint8_t c = 0;
    if (uartRxRingBufferReadPtr != UART_DMA_WRITE_PTR) {
        c = uartRxRingBuffer[uartRxRingBufferReadPtr++];
        uartRxRingBufferReadPtr &= (UART_RX_RING_BUFFER_SIZE - 1);
    }
    return c;
}

uint8_t UART_RxRingBufferPeek(uint8_t offset) {
    uint8_t c = 0;
    if (uartRxRingBufferReadPtr != UART_DMA_WRITE_PTR) {
        c = uartRxRingBuffer[uartRxRingBufferReadPtr + offset];
    }
    return c;
}

uint8_t UART_CalculateChecksum(uint8_t* data, uint8_t length) {
    uint8_t checksum = UART_CHECKSUM_START_VALUE;
    for (uint16_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

uint8_t UART_VerifyChecksum(uint8_t* data, uint8_t length) {
    uint8_t checksum = UART_CalculateChecksum(data, length - 1);
    return (checksum == data[length - 1]);
}

void UART_TransmitResponse(uint8_t* data, uint8_t length) {
    uartTxBuffer[0] = UART_START_BYTE;
    uartTxBuffer[1] = length + 1;
    memcpy(&uartTxBuffer[2], data, length);
    uartTxBuffer[length + 2] = UART_CalculateChecksum(data, length);
    HAL_UART_Transmit_DMA(&CONTROL_UART, uartTxBuffer, length + 3);
}

void UART_HandleProtocol(void) {
    // Skip if there's nothing to read
    if (UART_RxRingBufferEmpty())
        return;

    // Check if the minimum number of bytes for a command is available (otherwise reading is pointless)
    uint8_t inWaiting = UART_RxRingBufferWaiting();
    if (inWaiting < UART_MIN_COMMAND_LENGTH)
        return;

    // Cancel destructively if the first byte isn't a valid start byte
    if (UART_RxRingBufferPeek(0) != UART_START_BYTE) {
        UART_RxRingBufferRead();
        return;
    }

    // Check how many more bytes there are to be expected
    uint8_t length = UART_RxRingBufferPeek(1);

    // Cancel destructively if the number of expected bytes is too large
    if (length > UART_MAX_PAYLOAD_LENGTH) {
        UART_RxRingBufferRead();
        return;
    }

    // Cancel non-destructively if we haven't received the whole command yet
    if (length > inWaiting)
        return;

    // If we have already received the whole command,
    // take the start byte and length byte we've peeked at from the buffer
    UART_RxRingBufferRead();
    UART_RxRingBufferRead();

    // Read the payload
    for (uint16_t i = 0; i < length; i++) {
        uartRxPayload[i] = UART_RxRingBufferRead();
    }

    // Check the checksum, cancel if it doesn't match
    if (!UART_VerifyChecksum(uartRxPayload, length))
        return;

    // Call the command handler
    UART_ProcessCommand(uartRxPayload[0], &uartRxPayload[1], length - 2);

    // Finally, clear the payload array
    memset(uartRxPayload, 0x00, UART_MAX_PAYLOAD_LENGTH);
}