#ifndef ADXL375_H_
#define ADXL375_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "datatypes.h"
#include "pinconfig.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32g4xx.h"


// Registers
#define ADXL_POWER_CTL 0x2DU
#define ADXL_BW_RATE 0x2CU

#define ADXL_INT_ENABLE 0x2EU
#define ADXL_INT_MAP 0x2FU

#define ADXL_FIFO_CTL 0x38U
#define ADXL_FIFO_STATUS 0x39U
#define ADXL_DATA 0x32U   // 6 contiguous registers for each FIFO word

#define ADXL_DEVID 0x00U


// Config options
#define ADXL_DR_100HZ 0b1010U
#define ADXL_DR_12_5HZ 0b0111U


// Other
#define ADXL_DR_FREQ 100   // Used for calculating timestamp
#define ADXL_PKT_DATA_LEN (2 + 4 * sizeof(float))


// Functions
// Initialise the ADXL but does NOT actually start taking measurements
bool InitialiseADXL375(SPI_HandleTypeDef *hspi, uint8_t WatermarkWords, bool Blocking);

// Set to standby mode where no measurements are taken
void ADXL375_SetStandby(SPI_HandleTypeDef *hspi);

// Set to measurement mode where measurements are taken at the configured data rate
void ADXL375_SetMeasure(SPI_HandleTypeDef *hspi);

// Sets FIFO to bypass mode for a short time to clear it and then resets it to streaming mode
void ADXL375_InitialiseFIFO(SPI_HandleTypeDef *hspi, uint8_t WatermarkWords, bool Blocking);

// Read the latest measurement
Vec3 ADXL375_ReadSingleAccelData(SPI_HandleTypeDef *hspi);

// Read batched data from the FIFO buffer
void ADXL375_ReadFIFOData(SPI_HandleTypeDef *hspi, TS_Vec3 *accbuff, uint8_t readnum, float readytime);

// Get the status of the FIFO buffer
uint8_t ADXL375_GetFIFOStatus(SPI_HandleTypeDef *hspi);

// Constructs a flash logging packet from the given data and appends it to the buffer
bool ADXL375_AppendLogPacket(SPI_HandleTypeDef *hspi, uint8_t *buff, uint16_t *BuffPos, uint16_t BuffMaxLen, TS_Vec3 *databuff, uint8_t Readings);


#endif /* ADXL375_H_ */
