#ifndef LAMBDA62_H_
#define LAMBDA62_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "datatypes.h"


// MCU connected pins
#define L62_CS_PORT GPIOA
#define L62_CS_PIN GPIO_PIN_15

#define L62_DIO1_PORT GPIOB
#define L62_DIO1_PIN GPIO_PIN_4

#define L62_DIO2_PORT GPIOC
#define L62_DIO2_PIN GPIO_PIN_2

#define L62_BUSY_PORT GPIOB
#define L62_BUSY_PIN GPIO_PIN_5

#define L62_RST_PORT GPIOC
#define L62_RST_PIN GPIO_PIN_13

#define L62_RXSW_PORT GPIOC
#define L62_RXSW_PIN GPIO_PIN_0

#define L62_TXSW_PORT GPIOC
#define L62_TXSW_PIN GPIO_PIN_1



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



// Functions
void InitialiseLAMBDA62();

inline bool LAMBDA62_CheckBusy();
void LAMBDA62_ClearIRQ(uint16_t IRQMask);

void LAMBDA62_SetTx(uint32_t Timeout);
void LAMBDA62_SendPacket(uint8_t *packet, uint8_t len);

void LAMBDA62_SetRx(uint32_t Timeout);
uint8_t LAMBDA62_ReadPacket(uint8_t *buff, uint8_t MaxLen);   // Returns actual read length (0 if packet length exceeds buffer length)

void LAMBDA62_SendContinuousWave();








#endif /* LAMBDA62_H_ */
