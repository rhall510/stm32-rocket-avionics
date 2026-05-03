#ifndef MAIN_H_
#define MAIN_H_

#include "stm32g4xx.h"
#include "debug_peripherals.h"
#include "adxl375.h"
#include "lsm6dsr.h"
#include "bmp581.h"
#include "mmc5983.h"
#include "m10s.h"
#include "w25q.h"
#include "lambda62.h"
#include "lambda80.h"
#include "datatypes.h"
#include "misc.h"
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>


I2C_HandleTypeDef hi2c;
SPI_HandleTypeDef hspi1_acc;
SPI_HandleTypeDef hspi2_str;
SPI_HandleTypeDef hspi3_rf;


#define LSM6_FIFO_READNUM 4
#define ADXL_FIFO_READNUM 4
#define BMP_FIFO_READNUM 1

#define DOWNLOAD_PKT_SYNC_WORD 0x88442211
#define DOWNLOAD_PKT_TERM 0x336699CC


// Polls for available data. Returns true if a sentence is found
bool Poll_MAXM10S();


// Flash logging
#define FLASH_BUFFER_LEN 1024
#define FLASH_LOG_RATE 10   // Hz at which logging should be executed (handled by TIM2)

// Write all accumulated data to flash and flip to accumulating in the other buffer
void WriteFullDataPacket();

// Transmit all data stored in flash over 2.4GHz
void TransmitStoredData();



void L80_SendTestPackets();

// System initialisation
void SystemClockConfig(void);

void InitialiseGPIO();
void InitialiseI2C();
void InitialiseSPI();
void InitialiseCRC();
void InitialiseTimers();


// Error handling
void Error_Handler(void);


#endif /* MAIN_H_ */
