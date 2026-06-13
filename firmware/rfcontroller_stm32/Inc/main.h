#ifndef MAIN_H_
#define MAIN_H_

#include "stm32g4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "tusb.h"

#include <stdint.h>
#include <stdio.h>

#include "lambda62.h"
#include "lambda80.h"
#include "pinconfig.h"
#include "networking.h"
#include "usbcmd.h"


SPI_HandleTypeDef hspi3_rf;


// Task notifications
TaskHandle_t USBRxTaskNotif = NULL;

// Mutexes
SemaphoreHandle_t USBTxMutex = NULL;

// Queue handles
QueueHandle_t CommandQueue;
QueueHandle_t RadioResponseQueue;




// Tasks
// Run the TinyUSB stack
void RunTUDTask(void *param);

// Read from the incoming USB stream and parse into packets which are pushed onto the command queue
void ReadIncomingUSB(void *param);


// Executes a state machine to handle all transaction types
void TransactionManagerTask(void *param);

// States for the transaction manager state machine
typedef enum {
	TM_STATE_IDLE,
	TM_ECHO_CMD,
	TM_STATUS_CMD,
    TM_NUM_STATES   // Not an actual state, just useful for getting the number of possible states
} TMState;

// Transaction manager state handler function signature
// All functions take in the most recent USB command and radio response packets and return the state the
// transaction manager should transition to
typedef TMState (*TMStateHandler)(USBPacket* pkt, NetPacket* resp);


// Transaction manager state handler functions
TMState HandleStateIdle(USBPacket* pkt, NetPacket* resp);
TMState HandleStateEchoCmd(USBPacket* pkt, NetPacket* resp);
TMState HandleStateStatusCmd(USBPacket* pkt, NetPacket* resp);


// USB packet functions
void SendPacketUSB(USBPacket* packetinfo);

typedef enum {
    USBPARSER_WAIT_SYNC_HIGH,
	USBPARSER_WAIT_SYNC_LOW,
	USBPARSER_TYPE,
	USBPARSER_LEN,
	USBPARSER_PAYLOAD
} USBParserState;

void ParseUSBBytes(uint8_t *bytestream, uint16_t len);









void SystemClockConfig(void);
void InitialiseGPIO();
void InitialiseSPI();

// Error handling
void Error_Handler(void);

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);

#endif /* MAIN_H_ */
