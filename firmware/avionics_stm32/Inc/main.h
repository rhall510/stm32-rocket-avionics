#ifndef MAIN_H_
#define MAIN_H_

#include "stm32g4xx.h"
#include "debug_peripherals.h"
#include "adxl375.h"
#include "lsm6dsr.h"
#include "bmp581.h"
#include "mmc5983.h"
#include "m10s.h"
#include "datatypes.h"
#include "minmea.h"
#include "misc.h"
#include <math.h>
#include <stdint.h>
#include <stdbool.h>


I2C_HandleTypeDef hi2c;
SPI_HandleTypeDef hspi1_acc;
SPI_HandleTypeDef hspi2_str;
SPI_HandleTypeDef hspi3_rf;


#define LSM6_FIFO_READNUM 4
#define ADXL_FIFO_READNUM 4
#define BMP_FIFO_READNUM 1


void Poll_MAXM10S();

void SystemClockConfig(void);

void InitialiseGPIO();
void InitialiseI2C();
void InitialiseSPI();

void Error_Handler(void);


#endif /* MAIN_H_ */
