#ifndef ADXL375_H_
#define ADXL375_H_

#include <stdbool.h>
#include <stdint.h>
#include "datatypes.h"


// MCU connected pins
#define ADXL_CS_PORT GPIOA
#define ADXL_CS_PIN GPIO_PIN_4

#define ADXL_INT1_PORT GPIOB
#define ADXL_INT1_PIN GPIO_PIN_0

#define ADXL_INT2_PORT GPIOB
#define ADXL_INT2_PIN GPIO_PIN_1


// Registers
#define ADXL_POWER_CTL 0x2DU
#define ADXL_BW_RATE 0x2CU

#define ADXL_INT_ENABLE 0x2EU
#define ADXL_INT_MAP 0x2FU

#define ADXL_FIFO_CTL 0x38U
#define ADXL_FIFO_STATUS 0x39U
#define ADXL_DATA 0x32U   // 6 contiguous registers for each FIFO word


// Other
#define ADXL_DR_FREQ 100   // Used for calculating timestamp



// Functions
void InitialiseADXL375(uint8_t WatermarkWords);

struct Vector3 ADXL375_ReadSingleAccelData();

void ADXL375_ReadFIFOData(volatile struct TS_Vec3 *accbuff, uint8_t readnum, float readytime);
uint8_t ADXL375_GetFIFOStatus();


#endif /* ADXL375_H_ */
