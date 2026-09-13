#ifndef MAIN_H_
#define MAIN_H_

#include "stm32g4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"

#include "pinconfig.h"
#include "adxl375.h"
#include "lsm6dsr.h"
#include "bmp581.h"
#include "mmc5983.h"
#include "m10s.h"
#include "w25q.h"
#include "lambda62.h"
#include "lambda80.h"

#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "datatypes.h"
#include "misc.h"
#include "networking.h"

#include "fusion.h"


I2C_HandleTypeDef hi2c;
SPI_HandleTypeDef hspi1_acc;
SPI_HandleTypeDef hspi2_str;
SPI_HandleTypeDef hspi3_rf;


#define NET_ADDRESS NET_AVIONICS_ADDR

#define TELEMETRY_PERIOD_MS 100   // Minimum ms between each telemetry packet sent


// Task notifications
TaskHandle_t LAMBDA80RxTaskNotif = NULL;
TaskHandle_t LAMBDA62RxTaskNotif = NULL;
TaskHandle_t LSM6DSRReadTaskNotif = NULL;
TaskHandle_t ADXL375ReadTaskNotif = NULL;
TaskHandle_t BMP581ReadTaskNotif = NULL;
TaskHandle_t MMC5983ReadTaskNotif = NULL;
TaskHandle_t M10SReadTaskNotif = NULL;
TaskHandle_t DataCollectionTaskNotif = NULL;
TaskHandle_t LogDataTaskNotif = NULL;

// Semaphores
SemaphoreHandle_t LAMBDA80TxSemphr = NULL;
SemaphoreHandle_t LAMBDA62TxSemphr = NULL;

// Mutexes
SemaphoreHandle_t SPIRfMutex = NULL;
SemaphoreHandle_t SPIAccMutex = NULL;
SemaphoreHandle_t SPIFlashMutex = NULL;
SemaphoreHandle_t I2CMutex = NULL;

// Queue handles
QueueHandle_t RadioQueue;
QueueHandle_t DataLogQueue;
QueueHandle_t FusionQueue;

// Timers and callback functions
TimerHandle_t M10SPollTimer;
void PrimeM10SPoll(TimerHandle_t Timer);

TimerHandle_t LogDataTimer;
void PrimeLogData(TimerHandle_t Timer);


// Tasks
// Read from radio modules and push the packet onto the radio receive queue
// TODO Uses DMA transfers for packets above RADIO_PKTLEN_DMA_THRESH bytes long
#define RADIO_PKTLEN_DMA_THRESH 100

void ReadIncomingLAMBDA80(void *param);
void ReadIncomingLAMBDA62(void *param);

// Executes a state machine to handle all transaction types
void TransactionManagerTask(void *param);

/// Read from sensors when data is ready
void ReadLSM6DSRTask(void *param);
void ReadADXL375Task(void *param);
void ReadBMP581Task(void *param);
void ReadMMC5983Task(void *param);
void ReadM10STask(void *param);   // Must be polled as it doesn't have a ready interrupt

// Enable or disable data collection
bool DataCollectionEnabled = false;
void TriggerDataCollectionTask(void *param);

// Log data to flash storage
void LogDataTask(void *param);

// Fuse sensor data into 3D estimate
void SensorFusionTask(void *param);


// States for the transaction manager state machine
typedef enum {
	TM_STATE_IDLE,
	TM_DISC_CMD,
	TM_PKTTEST_CMD,
	TM_DATARNG_CMD,
	TM_TRSMT_DATA_CMD,
	TM_TELEMETRY_CMD,
	TM_SETHOME,
    TM_NUM_STATES   // Not an actual state, just useful for getting the number of possible states
} TMState;

// Transaction manager state handler function signature
// All functions take in the most recent radio packet and return the state the transaction manager should transition to
typedef TMState (*TMStateHandler)(NetPacket* pkt);

// Transaction manager state handler functions
TMState HandleStateIdle(NetPacket* pkt);
TMState HandleStateDiscoveryCmd(NetPacket* pkt);
TMState HandleStatePktTestCmd(NetPacket* resp);
TMState HandleStateDataRangeCmd(NetPacket* resp);
TMState HandleStateTransmitDataCmd(NetPacket* resp);
TMState HandleStateTelemetryCmd(NetPacket* resp);
TMState HandleStateSetHomeCmd(NetPacket* resp);



// Sensor config
#define LSM6_FIFO_READNUM 4
#define ADXL_FIFO_READNUM 4
#define BMP_FIFO_READNUM 1

#define FLASH_BUFFER_LEN 1024

void SetDataCollectionEnabled(bool Collect);


// Samples needed for GPS and pressure to set home position. Home is set when both thresholds are met
#define SETHOME_MIN_GPS_SAMPLES 10
#define SETHOME_MIN_PRES_SAMPLES 10

bool SettingHome = false;   // For redirecting readings to home position calculation
bool HomePositionSet = false;   // Whether the home position has been initialised at least once since power on


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
