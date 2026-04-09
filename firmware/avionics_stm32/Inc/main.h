#ifndef MAIN_H_
#define MAIN_H_

#include "stm32g4xx.h"
#include "debug_peripherals.h"
#include "adxl375.h"
#include "lsm6dsr.h"

I2C_HandleTypeDef hi2c;
SPI_HandleTypeDef hspi1_acc;
SPI_HandleTypeDef hspi2_str;
SPI_HandleTypeDef hspi3_rf;


#define LSM6_FIFO_READWORDS 10


void SystemClockConfig(void);

void InitialiseGPIO();
void InitialiseI2C();
void InitialiseSPI();

void Error_Handler(void);


#endif /* MAIN_H_ */
