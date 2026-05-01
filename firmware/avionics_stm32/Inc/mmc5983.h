#ifndef MMC5983_H_
#define MMC5983_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "datatypes.h"


// MCU connected pins
#define MMC_INT_PORT GPIOA
#define MMC_INT_PIN GPIO_PIN_10


// Registers
#define MMC_ID 0x2FU
#define MMC_STATUS 0x08U

#define MMC_CTRL0 0x09U
#define MMC_CTRL1 0x0AU
#define MMC_CTRL2 0x0BU
#define MMC_CTRL3 0x0CU

#define MMC_DATA 0x00U   // 7 contiguous data registers for X[0:1], Y[2:3], Z[4:5] and XYZ [6]


// Other
#define MMC_I2C_ADDR (0x30 << 1)
#define MMC_PKT_DATA_LEN (2 + 4 * sizeof(float))


// Functions
bool InitialiseMMC5983MA();

uint8_t MMC5983MA_GetStatus();
void MMC5983MA_ReadData(volatile struct TS_Vec3 *mout, float readytime);

// Constructs a flash logging packet from the given data and appends it to the buffer
bool MMC5983MA_AppendLogPacket(uint8_t *buff, uint16_t *BuffPos, uint16_t BuffMaxLen, volatile struct TS_Vec3 *databuff, uint8_t Readings);

#endif /* MMC5983_H_ */
