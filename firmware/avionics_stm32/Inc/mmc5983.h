#ifndef MMC5983_H_
#define MMC5983_H_

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
#define MMC_ID 0x2FU
#define MMC_STATUS 0x08U

#define MMC_CTRL0 0x09U
#define MMC_CTRL1 0x0AU
#define MMC_CTRL2 0x0BU
#define MMC_CTRL3 0x0CU

#define MMC_DATA 0x00U   // 7 contiguous data registers for X[0:1], Y[2:3], Z[4:5] and XYZ [6]


// Config options
#define MMC_DR_100HZ 0b101U
#define MMC_DR_10HZ 0b010U
#define MMC_DR_1HZ 0b001U


// Other
#define MMC_I2C_ADDR (0x30 << 1)
#define MMC_PKT_DATA_LEN (2 + 4 * sizeof(float))


// Functions
// Initialises the MMC but does NOT actually start taking measurements
bool InitialiseMMC5983MA(I2C_HandleTypeDef *hi2c, bool Blocking);

// Perform software reset with a 15ms wakeup delay
void MMC5983MA_Reset(I2C_HandleTypeDef *hi2c, bool Blocking);

// Set the rate of continuous measurements
void MMC5983MA_SetMeasurement(I2C_HandleTypeDef *hi2c, uint8_t datarate);

// Get the current status of the device
uint8_t MMC5983MA_GetStatus(I2C_HandleTypeDef *hi2c);

// Read The latest measurement data
void MMC5983MA_ReadData(I2C_HandleTypeDef *hi2c, TS_Vec3 *mout, float readytime);

// Constructs a flash logging packet from the given data and appends it to the buffer
bool MMC5983MA_AppendLogPacket(I2C_HandleTypeDef *hi2c, uint8_t *buff, uint16_t *BuffPos, uint16_t BuffMaxLen, TS_Vec3 *databuff, uint8_t Readings);

#endif /* MMC5983_H_ */
