#ifndef MISC_H_
#define MISC_H_

#include "stm32g4xx.h"


void ScanI2CBus(I2C_HandleTypeDef hi2c);

#define GET_MICROS (TIM2->CNT)

#endif /* MISC_H_ */
