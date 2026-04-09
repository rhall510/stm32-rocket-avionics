#ifndef LSM6DSR_H_
#define LSM6DSR_H_

#include <stdbool.h>
#include <stdint.h>
#include "datatypes.h"


// MCU connected pins
#define LSM6_CS_PORT GPIOA
#define LSM6_CS_PIN GPIO_PIN_3

#define LSM6_INT1_PORT GPIOC
#define LSM6_INT1_PIN GPIO_PIN_4

#define LSM6_INT2_PORT GPIOC
#define LSM6_INT2_PIN GPIO_PIN_5



// Registers
#define LSM6_CTRL1_XL 0x10U
#define LSM6_CTRL2_G 0x11U
#define LSM6_CTRL3_C 0x12U
#define LSM6_CTRL10_C 0x19U
#define LSM6_FIFO_CTRL1 0x07U
#define LSM6_FIFO_CTRL2 0x08U
#define LSM6_FIFO_CTRL3 0x09U
#define LSM6_FIFO_CTRL4 0x0AU
#define LSM6_INT1_CTRL 0x0DU


#define LSM6_OUT_G 0x22U   // 3 contiguous two byte registers for each axis
#define LSM6_OUT_A 0x28U   // 3 contiguous two byte registers for each axis
#define LSM6_FIFO_DATA_OUT 0x78U   // 7 contiguous registers for each FIFO word
#define LSM6_FIFO_STATUS 0x3A   // 2 contiguous registers for full FIFO status


// Other
#define LSM6_FIFO_DATA_BLOCK_SIZE 3


// Functions
void InitialiseLSM6DSR(uint16_t WatermarkReads);

struct Vector3 LSM6DSR_ReadInstAccelData();
struct Vector3 LSM6DSR_ReadInstGyroData();

void LSM6DSR_ReadFIFOData(volatile struct TS_Vec3 *accbuff, volatile struct TS_Vec3 *gyrbuff, uint16_t readnum);
uint16_t LSM6DSR_GetFIFOStatus();


#endif /* LSM6DSR_H_ */
