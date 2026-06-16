#ifndef LAMBDA62_H_
#define LAMBDA62_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "pinconfig.h"
#include "stm32g4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "misc.h"


// Registers
#define L62_TXCLAMP 0x08D8U


// Opcodes
#define L62_SETTX 0x83U
#define L62_SETRX 0x82U
#define L62_SETCW 0xD1U
#define L62_STDBY 0x80U

#define L62_WRITE_REG 0x0DU
#define L62_READ_REG 0x1DU
#define L62_WRITE_BUFF 0x0EU
#define L62_READ_BUFF 0x1EU

#define L62_SET_IRQ 0x08U
#define L62_IRQ_STATUS 0x12U
#define L62_CLEAR_IRQ 0x02U

#define L62_RF_FREQ 0x86U
#define L62_PA_CFG 0x95U
#define L62_BUFF_BASE_ADDR 0x8FU
#define L62_PKT_TYPE 0x8AU
#define L62_TX_PARAMS 0x8EU
#define L62_MOD_PARAMS 0x8BU
#define L62_PKT_PARAMS 0x8CU

#define L62_RX_BUFF_STATUS 0x13U
#define L62_PKT_STATUS 0x14U
#define L62_STATUS 0xC0U

#define L62_INST_RSSI 0x15U
#define L62_STATS 0x10U
#define L62_RESET_STATS 0x00U


// Other
#define L62_TX_TIMEOUT 0x0U   // 0x0 to disable timeout
#define L62_RX_TIMEOUT 0xFFFFFFU   // 0x0 to disable timeout (single mode), 0xFFFFFF for continuous mode

#define L62_TX_BASE_ADDR 0x0U   // Buffer base address for transmit packets
#define L62_RX_BASE_ADDR 0x80U   // Buffer base address for receive packets

// FXTAL is 32MHz

// Functions
// Returns the state of the BUSY pin
bool LAMBDA62_CheckBusy();

// Blocks downstream code from running until the BUSY pin goes low. If 'Blocking' is true it will execute a blocking wait, otherwise
// it will use a short 50us blocking polling loop to catch quick events followed by repeating 1ms FreeRTOS task delays
void LAMBDA62_WaitBusy(bool Blocking);

// Initialise the LAMBDA62 to send or receive LoRa packets
void InitialiseLAMBDA62LoRa(SPI_HandleTypeDef *hspi, bool Blocking);

// Initialise the LAMBDA62 to send or receive GFSK packets
void InitialiseLAMBDA62FSK(SPI_HandleTypeDef *hspi, bool Blocking);

// Clear all interrupt flags
void LAMBDA62_ClearIRQ(SPI_HandleTypeDef *hspi, uint16_t IRQMask, bool Blocking);

// Set to Tx mode and transmit the contents of the buffer
void LAMBDA62_SetTx(SPI_HandleTypeDef *hspi, uint32_t Timeout, bool Blocking);

// Writes the provided packet data into the Tx buffer and transmits it
void LAMBDA62_SendPacket(SPI_HandleTypeDef *hspi, uint8_t *packet, uint8_t len, bool Blocking);

void LAMBDA62_SetPacketParamsLoRa(SPI_HandleTypeDef *hspi, uint16_t PreambleLen, uint8_t HeaderType, uint8_t len,
								  uint8_t CRCType, uint8_t InvertIQ, bool Blocking);

void LAMBDA62_SetPacketParamsFSK(SPI_HandleTypeDef *hspi, uint16_t PreambleLen, uint8_t PreambleDetectLen, uint8_t SyncWordLen,
								 uint8_t AddrComp, bool ExplicitLength, uint8_t len, uint8_t CRCType, bool Whitening, bool Blocking);

// Set to Rx mode
void LAMBDA62_SetRx(SPI_HandleTypeDef *hspi, uint32_t Timeout, bool Blocking);

// Get the start position and length of the packet stored in the Rx buffer
void LAMBDA62_GetRxBufferStatus(SPI_HandleTypeDef *hspi, uint8_t *len, uint8_t *start, bool Blocking);

// Reads the Rx buffer into the provided buffer
void LAMBDA62_ReadBuffer(SPI_HandleTypeDef *hspi, uint8_t *buff, uint8_t StartAddr, uint8_t len, bool Blocking);

uint8_t LAMBDA62_ReadReg(SPI_HandleTypeDef *hspi, uint16_t RegAddr, bool Blocking);
void LAMBDA62_WriteReg(SPI_HandleTypeDef *hspi, uint16_t RegAddr, uint8_t val, bool Blocking);

void LAMBDA62_SendContinuousWave(SPI_HandleTypeDef *hspi, bool Blocking);


#endif /* LAMBDA62_H_ */
