#ifndef LSM6DSR_H_
#define LSM6DSR_H_

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
#define LSM6_CTRL1_XL 0x10U
#define LSM6_CTRL2_G 0x11U
#define LSM6_CTRL3_C 0x12U
#define LSM6_CTRL4_C 0x13U
#define LSM6_CTRL9_XL 0x18U
#define LSM6_CTRL10_C 0x19U
#define LSM6_FIFO_CTRL1 0x07U
#define LSM6_FIFO_CTRL2 0x08U
#define LSM6_FIFO_CTRL3 0x09U
#define LSM6_FIFO_CTRL4 0x0AU
#define LSM6_INT1_CTRL 0x0DU
#define LSM6_INT2_CTRL 0x0EU


#define LSM6_OUT_G 0x22U   // 3 contiguous two byte registers for each axis
#define LSM6_OUT_A 0x28U   // 3 contiguous two byte registers for each axis
#define LSM6_FIFO_DATA_OUT 0x78U   // 7 contiguous registers for each FIFO word
#define LSM6_FIFO_STATUS 0x3AU   // 2 contiguous registers for full FIFO status

#define LSM6_WHO_AM_I 0x0FU


// Other
#define LSM6_FIFO_DATA_BLOCK_SIZE 3
#define LSM6_PKT_DATA_LEN (2 + 4 * sizeof(float))


// Config options
#define LSM6_DR_NONE 0b0000U
#define LSM6_DR_104HZ 0b0100U
#define LSM6_DR_12_5HZ 0b0001U

#define LSM6_ACCRNG_16G 0b01U
#define LSM6_ACCRNG_8G 0b11U
#define LSM6_ACCRNG_4G 0b10U
#define LSM6_ACCRNG_2G 0b00U

#define LSM6_ACCFILT_FS 0b0U
#define LSM6_ACCFILT_SS 0b0U

#define LSM6_GYRRNG_2000DPS 0b11U
#define LSM6_GYRRNG_1000DPS 0b10U
#define LSM6_GYRRNG_500DPS 0b01U
#define LSM6_GYRRNG_250DPS 0b00U




// Functions
// Initialises the LSM6DSR but does NOT actually start taking measurements
bool InitialiseLSM6DSR(SPI_HandleTypeDef *hspi, uint16_t WatermarkReads, bool Blocking);

// Perform software reset with a 5ms wakeup delay
void LSM6DSR_Reset(SPI_HandleTypeDef *hspi, bool Blocking);

// Set the data rate, range, and filtering for the accelerometer and gyroscope
void LSM6DSR_SetMeasurementMode(SPI_HandleTypeDef *hspi, uint8_t accrate, uint8_t accrange, uint8_t accfilt, uint8_t gyrorate, uint8_t gyrorange);

// Read the latest measurements of the IMU
Vec3 LSM6DSR_ReadInstAccelData(SPI_HandleTypeDef *hspi);
Vec3 LSM6DSR_ReadInstGyroData(SPI_HandleTypeDef *hspi);

// Read batched data from the FIFO buffer
void LSM6DSR_ReadFIFOData(SPI_HandleTypeDef *hspi, TS_Vec3 *accbuff, TS_Vec3 *gyrbuff, uint16_t readnum, float readytime);

// Get the status of the FIFO buffer
uint16_t LSM6DSR_GetFIFOStatus(SPI_HandleTypeDef *hspi);

// Constructs a flash logging packet from the given data and appends it to the buffer
bool LSM6DSR_AppendLogPacket(uint8_t *buff, uint16_t *BuffPos, uint16_t BuffMaxLen, TS_Vec3 *accbuff, TS_Vec3 *gyrbuff, uint8_t Readings);

#endif /* LSM6DSR_H_ */
