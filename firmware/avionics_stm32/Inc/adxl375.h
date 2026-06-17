#ifndef ADXL375_H_
#define ADXL375_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "datatypes.h"
#include "pinconfig.h"


// Registers
#define ADXL_POWER_CTL 0x2DU
#define ADXL_BW_RATE 0x2CU

#define ADXL_INT_ENABLE 0x2EU
#define ADXL_INT_MAP 0x2FU

#define ADXL_FIFO_CTL 0x38U
#define ADXL_FIFO_STATUS 0x39U
#define ADXL_DATA 0x32U   // 6 contiguous registers for each FIFO word

#define ADXL_DEVID 0x00U


// Other
#define ADXL_DR_FREQ 100   // Used for calculating timestamp
#define ADXL_PKT_DATA_LEN (2 + 4 * sizeof(float))


// Functions
bool InitialiseADXL375(uint8_t WatermarkWords);
void ADXL375_SetStandby();

Vec3 ADXL375_ReadSingleAccelData();

void ADXL375_ReadFIFOData(volatile TS_Vec3 *accbuff, uint8_t readnum, float readytime);
uint8_t ADXL375_GetFIFOStatus();

// Constructs a flash logging packet from the given data and appends it to the buffer
bool ADXL375_AppendLogPacket(uint8_t *buff, uint16_t *BuffPos, uint16_t BuffMaxLen, volatile TS_Vec3 *databuff, uint8_t Readings);


#endif /* ADXL375_H_ */
