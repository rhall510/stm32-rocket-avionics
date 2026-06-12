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
#include "pinconfig.h"


SPI_HandleTypeDef hspi3_rf;


TaskHandle_t xUSBRx = NULL;


void SendPacketUSB(uint8_t *buff, uint16_t len);


void SystemClockConfig(void);
void InitialiseGPIO();
void InitialiseSPI();

// Error handling
void Error_Handler(void);

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);

#endif /* MAIN_H_ */
