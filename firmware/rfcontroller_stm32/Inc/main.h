#ifndef MAIN_H_
#define MAIN_H_

#include "stm32g4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "tusb.h"
#include "timers.h"

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
TaskHandle_t LAMBDA80RxTaskNotif = NULL;
TaskHandle_t LAMBDA62RxTaskNotif = NULL;

// Semaphores
SemaphoreHandle_t LAMBDA80TxSemphr = NULL;
SemaphoreHandle_t LAMBDA62TxSemphr = NULL;

// Mutexes
SemaphoreHandle_t USBTxMutex = NULL;
SemaphoreHandle_t SPIRfMutex = NULL;

// Queue handles
QueueHandle_t CommandQueue;
QueueHandle_t RadioResponseQueue;




// Tasks
// Run the TinyUSB stack
void RunTUDTask(void *param);

// Read from the incoming USB stream and parse into packets which are pushed onto the command queue
void ReadIncomingUSB(void *param);

// Read from radio modules and push the packet onto the radio receive queue
// TODO Uses DMA transfers for packets above RADIO_PKTLEN_DMA_THRESH bytes long
#define RADIO_PKTLEN_DMA_THRESH 100

void ReadIncomingLAMBDA80(void *param);
void ReadIncomingLAMBDA62(void *param);

// Executes a state machine to handle all transaction types
void TransactionManagerTask(void *param);

// States for the transaction manager state machine
typedef enum {
	TM_STATE_IDLE,
	TM_ECHO_CMD,
	TM_STATUS_CMD,
	TM_DISC_CMD,
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
TMState HandleStateDiscoveryCmd(USBPacket* pkt, NetPacket* resp);



// Periodic discovery packets
TimerHandle_t DiscoveryTimer;

void SendDiscoveryPacket(TimerHandle_t Timer);


// USB packet functions
// Formats and sends the provided packet to the host PC over USB
void SendPacketUSB(USBPacket* packetinfo);

// Parses full USB packets from a raw bytestream and adds them to the command queue
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
void InitialiseTimers();

// Error handling
void Error_Handler(void);

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);

#endif /* MAIN_H_ */
