#ifndef M10S_H_
#define M10S_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "datatypes.h"


// MCU connected pins
#define M10S_RST_PORT GPIOA
#define M10S_RST_PIN GPIO_PIN_12


// Registers
#define M10S_AVAIL 0xFDU
#define M10S_DATA 0xFFU


// Other
#define M10S_I2C_ADDR (0x42 << 1)
#define M10S_LINE_BUFFER_SIZE 128

// Functions
void InitialiseMAXM10S();

bool MAXM10S_SendCommand(uint8_t *cmd, uint16_t length);

uint16_t MAXM10S_GetAvailableBytes();
bool MAXM10S_ReadStream(uint8_t *buffer, uint16_t length);


#endif /* M10S_H_ */
