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





// Decode and print the chip mode + command status from a raw GetStatus byte
void LAMBDA62_PrintStatus(uint8_t status, const char *label);


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
#define L62_IMG_CAL 0x98U
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
#define L62_DEV_ERRORS 0x17U


// Config options
#define L62_TX_TIMEOUT 0x0U   // 0x0 to disable timeout
#define L62_RX_TIMEOUT 0xFFFFFFU   // 0x0 to disable timeout (single mode), 0xFFFFFF for continuous mode

#define L62_TX_BASE_ADDR 0x0U   // Buffer base address for transmit packets
#define L62_RX_BASE_ADDR 0x0U   // Buffer base address for receive packets

#define L62_PKTTYPE_LORA 0x01U
#define L62_PKTTYPE_GFSK 0x00U

#define L62_LORA_BW_7 0x00U
#define L62_LORA_BW_15 0x01U
#define L62_LORA_BW_31 0x02U
#define L62_LORA_BW_62 0x03U
#define L62_LORA_BW_125 0x04U
#define L62_LORA_BW_250 0x05U
#define L62_LORA_BW_500 0x06U

#define L62_LORA_CR_4_5 0x01U

#define L62_LORA_LDR_OFF 0x00U
#define L62_LORA_LDR_ON 0x01U


#define L62_LORA_PRMBL_LEN 16

#define L62_LORA_EXP_HDR 0x00U
#define L62_LORA_IMP_HDR 0x01U

#define L62_LORA_CRC_OFF 0x00U
#define L62_LORA_CRC_ON 0x01U

#define L62_LORA_STD_IQ 0x00U
#define L62_LORA_INV_IQ 0x01U



#define L62_GFSK_SHAPE_GBT0_5 0x09U

#define L62_GFSK_BW_7 0x0FU
#define L62_GFSK_BW_14 0x0EU
#define L62_GFSK_BW_29 0x0DU
#define L62_GFSK_BW_58 0x0CU
#define L62_GFSK_BW_117 0x0BU
#define L62_GFSK_BW_232 0x0AU
#define L62_GFSK_BW_467 0x09U



#define L62_GFSK_PRMBL_LEN 32

#define L62_GFSK_PRMBL_DET_OFF 0x00U
#define L62_GFSK_PRMBL_DET_8 0x04U
#define L62_GFSK_PRMBL_DET_16 0x05U
#define L62_GFSK_PRMBL_DET_24 0x06U
#define L62_GFSK_PRMBL_DET_32 0x07U

#define L62_GFSK_SYNC_LEN 64

#define L62_GFSK_ADDRFILT_OFF 0x00U
#define L62_GFSK_ADDRFILT_NODE 0x01U
#define L62_GFSK_ADDRFILT_BOTH 0x02U

#define L62_GFSK_CRC_2BYTE 0x02U


// FXTAL is 32MHz

// Functions
// Get the current status of the module
uint8_t LAMBDA62_Status(SPI_HandleTypeDef *hspi, bool Blocking);

// Get the status of any device errors
uint16_t LAMBDA62_DevErrors(SPI_HandleTypeDef *hspi, bool Blocking);

// Returns the state of the BUSY pin
bool LAMBDA62_CheckBusy();

// Blocks downstream code from running until the BUSY pin goes low. If 'Blocking' is true it will execute a blocking wait, otherwise
// it will use a short 50us blocking polling loop to catch quick events followed by repeating 1ms FreeRTOS task delays
void LAMBDA62_WaitBusy(bool Blocking);

// Initialise the LAMBDA62 to send or receive LoRa packets
void InitialiseLAMBDA62LoRa(SPI_HandleTypeDef *hspi, bool Blocking);

// Initialise the LAMBDA62 to send or receive GFSK packets
void InitialiseLAMBDA62FSK(SPI_HandleTypeDef *hspi, bool Blocking);

// Used by the above functions to initialise parts common to both protocols
void InitialiseLAMBDA62Common(SPI_HandleTypeDef *hspi, bool Blocking);

// Set IRQ sources
void LAMBDA62_SetIRQ(SPI_HandleTypeDef *hspi, uint16_t IRQMask, uint16_t DIO1Mask, uint16_t DIO2Mask, uint16_t DIO3Mask,  bool Blocking);

// Clear all interrupt flags
void LAMBDA62_ClearIRQ(SPI_HandleTypeDef *hspi, uint16_t IRQMask, bool Blocking);

// Get interrupt status
uint16_t LAMBDA62_GetIRQStatus(SPI_HandleTypeDef *hspi, bool Blocking);

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

// Get the status of the most recently received packet
void LAMBDA62_GetPktStatusLoRa(SPI_HandleTypeDef *hspi, int8_t *rssi, int8_t *snr, int8_t *sigrssi, bool Blocking);
void LAMBDA62_GetPktStatusFSK(SPI_HandleTypeDef *hspi, uint8_t *rxstatus, int8_t *rssisync, int8_t *rssiavg, bool Blocking);


uint8_t LAMBDA62_ReadReg(SPI_HandleTypeDef *hspi, uint16_t RegAddr, bool Blocking);
void LAMBDA62_WriteReg(SPI_HandleTypeDef *hspi, uint16_t RegAddr, uint8_t val, bool Blocking);

void LAMBDA62_WriteRegBurst(SPI_HandleTypeDef *hspi, uint16_t RegAddr, uint8_t *vals, uint8_t len, bool Blocking);
void LAMBDA62_ReadRegBurst(SPI_HandleTypeDef *hspi, uint16_t RegAddr, uint8_t *vals, uint8_t len, bool Blocking);

void LAMBDA62_SendContinuousWave(SPI_HandleTypeDef *hspi, bool Blocking);

// Get the instantaneous RSSI
int8_t LAMBDA62_GetInstRSSI(SPI_HandleTypeDef *hspi, bool Blocking);


#endif /* LAMBDA62_H_ */
