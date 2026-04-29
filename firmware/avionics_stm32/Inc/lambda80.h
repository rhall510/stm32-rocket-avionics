#ifndef LAMBDA80_H_
#define LAMBDA80_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "datatypes.h"

// MCU connected pins
#define L80_CS_PORT GPIOD
#define L80_CS_PIN GPIO_PIN_2

#define L80_DIO1_PORT GPIOB
#define L80_DIO1_PIN GPIO_PIN_6

#define L80_DIO2_PORT GPIOC
#define L80_DIO2_PIN GPIO_PIN_3

#define L80_BUSY_PORT GPIOB
#define L80_BUSY_PIN GPIO_PIN_7

#define L80_RST_PORT GPIOC
#define L80_RST_PIN GPIO_PIN_14


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
#define L80_RX_TIMEOUT 0xFFFFFFU   // 0x0 to disable timeout (single mode), 0xFFFFFF for continuous mode

#define L80_TX_BASE_ADDR 0x0U   // Buffer base address for transmit packets
#define L80_RX_BASE_ADDR 0x80U   // Buffer base address for receive packets



// Functions
void InitialiseLAMBDA62();

inline bool LAMBDA80_CheckBusy();
void LAMBDA80_ClearIRQ(uint16_t IRQMask);

void LAMBDA80_SetTx(uint32_t Timeout);
void LAMBDA80_SendPacket(uint8_t *packet, uint8_t len);
void LAMBDA80_SetPacketParams(uint16_t PreambleLen, uint8_t HeaderType, uint8_t len, uint8_t CRCType, uint8_t InvertIQ);

void LAMBDA80_SetRx(uint32_t Timeout);
void LAMBDA80_GetRxBufferStatus(uint8_t *len, uint8_t *start);
void LAMBDA80_ReadBuffer(uint8_t *buff, uint8_t StartAddr, uint8_t len);

uint8_t LAMBDA80_ReadReg(uint16_t RegAddr);
void LAMBDA80_WriteReg(uint16_t RegAddr, uint8_t val);

void LAMBDA80_SendContinuousWave();


#endif /* LAMBDA80_H_ */
