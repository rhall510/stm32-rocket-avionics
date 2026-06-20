#ifndef LAMBDA80_H_
#define LAMBDA80_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "stm32g4xx.h"
#include "pinconfig.h"
#include "FreeRTOS.h"
#include "task.h"
#include "misc.h"


// Registers
#define L80_TXCLAMP 0x08D8U


// Opcodes
#define L80_SETTX 0x83U
#define L80_SETRX 0x82U
#define L80_SETCW 0xD1U
#define L80_STDBY 0x80U

#define L80_WRITE_REG 0x18U
#define L80_READ_REG 0x19U
#define L80_WRITE_BUFF 0x1AU
#define L80_READ_BUFF 0x1BU

#define L80_SET_IRQ 0x8DU
#define L80_IRQ_STATUS 0x15U
#define L80_CLEAR_IRQ 0x97U

#define L80_RF_FREQ 0x86U
#define L80_BUFF_BASE_ADDR 0x8FU
#define L80_PKT_TYPE 0x8AU
#define L80_TX_PARAMS 0x8EU
#define L80_MOD_PARAMS 0x8BU
#define L80_PKT_PARAMS 0x8CU

#define L80_RX_BUFF_STATUS 0x17U
#define L80_PKT_STATUS 0x1DU
#define L80_STATUS 0xC0U

#define L80_INST_RSSI 0x1FU


// Other
#define L80_TX_TIMEOUT 0x0U   // 0x0 to disable timeout
#define L80_RX_TIMEOUT 0xFFFFU   // 0x0 to disable timeout (single mode), 0xFFFF for continuous mode

#define L80_TX_BASE_ADDR 0x0U   // Buffer base address for transmit packets
#define L80_RX_BASE_ADDR 0x80U   // Buffer base address for receive packets



// Functions
// Get the current status of the module
uint8_t LAMBDA80_Status(SPI_HandleTypeDef *hspi, bool Blocking);

// Returns the state of the BUSY pin
bool LAMBDA80_CheckBusy();

// Blocks downstream code from running until the BUSY pin goes low. If 'Blocking' is true it will execute a blocking wait, otherwise
// it will use a short 50us blocking polling loop to catch quick events followed by repeating 1ms FreeRTOS task delays
void LAMBDA80_WaitBusy(bool Blocking);

// Initialise some basic configuration options for the LAMBDA80
void InitialiseLAMBDA80(SPI_HandleTypeDef *hspi, bool Blocking);

// Use optimised configuration for long range low data rate reception of telemetry packets
void LAMBDA80_SetMode_Telemetry(SPI_HandleTypeDef *hspi, bool Blocking);

// Use optimised configuration for short range high data rate sending of packets
void LAMBDA80_SetMode_Download(SPI_HandleTypeDef *hspi, bool Blocking);

// Clear all interrupt flags
void LAMBDA80_ClearIRQ(SPI_HandleTypeDef *hspi, uint16_t IRQMask, bool Blocking);

// Get interrupt status
uint16_t LAMBDA80_GetIRQStatus(SPI_HandleTypeDef *hspi, bool Blocking);

// Set to Tx mode and transmit the contents of the buffer
void LAMBDA80_SetTx(SPI_HandleTypeDef *hspi, uint8_t TimeBase, uint16_t Timeout, bool Blocking);

// Writes the provided packet data into the Tx buffer and transmits it
void LAMBDA80_SendPacket(SPI_HandleTypeDef *hspi, uint8_t *packet, uint8_t len, bool Blocking);

void LAMBDA80_SetPacketParams(SPI_HandleTypeDef *hspi, uint8_t PreambleLen, uint8_t HeaderType, uint8_t len,
							  uint8_t CRCType, uint8_t InvertIQ, bool Blocking);

// Set to Rx mode
void LAMBDA80_SetRx(SPI_HandleTypeDef *hspi, uint8_t TimeBase, uint16_t Timeout, bool Blocking);

// Get the start position and length of the packet stored in the Rx buffer
void LAMBDA80_GetRxBufferStatus(SPI_HandleTypeDef *hspi, uint8_t *len, uint8_t *start, bool Blocking);

// Reads the Rx buffer into the provided buffer
void LAMBDA80_ReadBuffer(SPI_HandleTypeDef *hspi, uint8_t *buff, uint8_t StartAddr, uint8_t len, bool Blocking);

// Get the status of the most recently received packet
void LAMBDA80_GetPktStatusLoRa(SPI_HandleTypeDef *hspi, int8_t *rssiSync, int8_t *snr, bool Blocking);
void LAMBDA80_GetPktStatusFSK(SPI_HandleTypeDef *hspi, int8_t *rssiSync, uint8_t *errors, uint8_t *status, uint8_t *sync, bool Blocking);

uint8_t LAMBDA80_ReadReg(SPI_HandleTypeDef *hspi, uint16_t RegAddr, bool Blocking);
void LAMBDA80_WriteReg(SPI_HandleTypeDef *hspi, uint16_t RegAddr, uint8_t val, bool Blocking);


#endif /* LAMBDA80_H_ */
