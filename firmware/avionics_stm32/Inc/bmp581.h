#ifndef BMP581_H_
#define BMP581_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "datatypes.h"
#include "pinconfig.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32g4xx.h"


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

#define BMP_RST_VAL 0xB6U   // This value must be written to the BMP_CMD register to trigger a soft reset


// Config options
#define BMP_DR_100Hz 0xAU
#define BMP_DR_10HZ 0x17U
#define BMP_DR_1HZ 0x1CU

#define BMP_PWR_STDBY 0b00U
#define BMP_PWR_NORMAL 0b01U
#define BMP_PWR_FORCED 0b10U
#define BMP_PWR_CONTINUOUS 0b11U


// Other
#define BMP_I2C_ADDR (0x46U << 1)

#define BMP_FIFO_DATA_BLOCK_SIZE 6
#define BMP_DR_FREQ 10   // Used for calculating timestamp
#define BMP_PKT_DATA_LEN (2 + 3 * sizeof(float))




// Functions
// Initialises the BMP581 but does NOT actually start taking measurements
bool InitialiseBMP581(I2C_HandleTypeDef *hi2c, uint8_t Threshold, bool Blocking);

// Perform software reset with a 10ms wakeup delay
void BMP581_Reset(I2C_HandleTypeDef *hi2c, bool Blocking);

// Set to standby mode where no measurements are taken
void BMP581_SetStandby(I2C_HandleTypeDef *hi2c);

// Set to normal mode where measurements are taken at the configured data rate
void BMP581_SetMeasure(I2C_HandleTypeDef *hi2c, uint8_t datarate);

// Get the number of unread samples in the FIFO buffer
uint8_t BMP581_GetFIFOCount(I2C_HandleTypeDef *hi2c);

// Read batched data from the FIFO buffer
void BMP581_ReadFIFOData(I2C_HandleTypeDef *hi2c, TS_PressTemp *ptbuff, uint8_t readnum, float readytime);

// Constructs a flash logging packet from the given data and appends it to the buffer
bool BMP581_AppendLogPacket(uint8_t *buff, uint16_t *BuffPos, uint16_t BuffMaxLen, TS_PressTemp *databuff, uint8_t Readings);

#endif /* BMP581_H_ */
