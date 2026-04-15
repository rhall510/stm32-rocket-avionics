#ifndef BMP581_H_
#define BMP581_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "datatypes.h"


// MCU connected pins
#define BMP_INT_PORT GPIOA
#define BMP_INT_PIN GPIO_PIN_11


// Registers
#define BMP_ASIC_ID 0x01U
#define BMP_STATUS 0x28U
#define BMP_INT_STATUS 0x27U
#define BMP_INT_SOURCE 0x15U
#define BMP_INT_CONF 0x14U
#define BMP_CMD 0x7EU

#define BMP_FIFO_CONF 0x16U
#define BMP_FIFO_COUNT 0x17U
#define BMP_FIFO_SEL 0x18U
#define BMP_FIFO_DATA 0x29U

#define BMP_OSR 0x36U
#define BMP_ODR 0x37U


// Other
#define BMP_I2C_ADDR (0x46U << 1)
#define BMP_FIFO_DATA_BLOCK_SIZE 6
#define BMP_DR_FREQ 10   // Used for calculating timestamp


// Functions
bool InitialiseBMP581(uint8_t Threshold);

uint8_t BMP581_GetFIFOCount();
void BMP581_ReadFIFOData(volatile struct TS_PressTemp *ptbuff, uint8_t readnum, float readytime);


#endif /* BMP581_H_ */
