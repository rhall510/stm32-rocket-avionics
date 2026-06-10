#ifndef MAIN_H_
#define MAIN_H_

#include "stm32g4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tusb.h"

#include <stdint.h>
#include <stdio.h>

#include "lambda62.h"
#include "lambda80.h"
#include "debug_peripherals.h"


SPI_HandleTypeDef hspi3_rf;
PCD_HandleTypeDef hpcd;




void SystemClockConfig(void);
void InitialiseGPIO();
void InitialiseSPI();

// Error handling
void Error_Handler(void);

#endif /* MAIN_H_ */
