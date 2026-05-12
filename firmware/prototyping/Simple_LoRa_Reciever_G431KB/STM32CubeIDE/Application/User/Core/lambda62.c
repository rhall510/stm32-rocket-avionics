#include "lambda62.h"
#include "stm32g4xx.h"


extern SPI_HandleTypeDef hspi3_rf;


inline bool LAMBDA62_CheckBusy() {
	return HAL_GPIO_ReadPin(L62_BUSY_PORT, L62_BUSY_PIN);
}


void LAMBDA62_ClearIRQ(uint16_t IRQMask) {
	uint8_t tx[3] = {L62_CLEAR_IRQ, (uint8_t)(IRQMask >> 8), (uint8_t)IRQMask};

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 3, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);
}


void InitialiseLAMBDA62() {
	uint8_t tx[5] = {0};

	// Hardware reset
	HAL_GPIO_WritePin(L62_RST_PORT, L62_RST_PIN, GPIO_PIN_RESET);
	HAL_Delay(1);
	HAL_GPIO_WritePin(L62_RST_PORT, L62_RST_PIN, GPIO_PIN_SET);

	while (LAMBDA62_CheckBusy()) {}   // Wait for startup


	// Set packet type to LoRa
	tx[0] = L62_PKT_TYPE;
	tx[1] = 0x1;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	while(LAMBDA62_CheckBusy()) {}

	// Set frequency to 868MHz
	tx[0] = L62_RF_FREQ;
	tx[1] = 0x36;
	tx[2] = 0x40;
	tx[3] = 0x00;
	tx[4] = 0x00;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 5, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	while(LAMBDA62_CheckBusy()) {}

	// Set PA config (DutyCycle, hpMax, deviceSel, paLut)
	tx[0] = L62_PA_CFG;
	tx[1] = 0x04;
	tx[2] = 0x07;
	tx[3] = 0x00;
	tx[4] = 0x01;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 5, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	while(LAMBDA62_CheckBusy()) {}

	// Set TX params (Power, RampTime)
	tx[0] = L62_TX_PARAMS;
	tx[1] = 0x16;
	tx[2] = 0x04;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 3, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	while(LAMBDA62_CheckBusy()) {}

	// Optimise TxClampConfig to minimise losses in case of antenna mismatch (as per datasheet section 15.2)
	uint8_t txconfig = LAMBDA62_ReadReg(L62_TXCLAMP);
	txconfig |= 0x1E;
	LAMBDA62_WriteReg(L62_TXCLAMP, txconfig);

	while(LAMBDA62_CheckBusy()) {}

	// Set buffer base addresses (TX, RX)
	tx[0] = L62_BUFF_BASE_ADDR;
	tx[1] = L62_TX_BASE_ADDR;
	tx[2] = L62_RX_BASE_ADDR;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 3, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	while(LAMBDA62_CheckBusy()) {}

	// Set modulation params (SF, BW, CR, LDR_OP)
	tx[0] = L62_MOD_PARAMS;
	tx[1] = 0x07;
	tx[2] = 0x04;
	tx[3] = 0x01;
	tx[4] = 0x00;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 5, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	// Set default packet parameters
	LAMBDA62_SetPacketParams(8, 0, 10, 1, 0);   // 8 bit preamble, explicit header, CRC on, normal IQ

	while(LAMBDA62_CheckBusy()) {}

	// Set Tx done interrupt on DIO1 and Rx done interrupt on DIO2
	uint8_t irq[9] = {L62_SET_IRQ, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00};

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, irq, 9, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	while(LAMBDA62_CheckBusy()) {}
}


void LAMBDA62_SetTx(uint32_t Timeout) {
	while(LAMBDA62_CheckBusy()) {}

	HAL_GPIO_WritePin(L62_RXSW_PORT, L62_RXSW_PIN, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(L62_TXSW_PORT, L62_TXSW_PIN, GPIO_PIN_SET);

	uint8_t tx[4] = {L62_SETTX, (uint8_t)(Timeout >> 16), (uint8_t)(Timeout >> 8), (uint8_t)Timeout};

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);
}


void LAMBDA62_SendPacket(uint8_t *packet, uint8_t len) {
	while(LAMBDA62_CheckBusy()) {}

	// Send opcode with offset then packet buffer
	uint8_t tx[2] = {L62_WRITE_BUFF, L62_TX_BASE_ADDR};

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 2, HAL_MAX_DELAY);
	HAL_SPI_Transmit(&hspi3_rf, packet, len, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	LAMBDA62_SetTx(L62_TX_TIMEOUT);
}


void LAMBDA62_SetPacketParams(uint16_t PreambleLen, uint8_t HeaderType, uint8_t len, uint8_t CRCType, uint8_t InvertIQ) {
	while(LAMBDA62_CheckBusy()) {}

	uint8_t tx[7] = {L62_PKT_PARAMS, (uint8_t)(PreambleLen >> 8), (uint8_t)PreambleLen, HeaderType, len, CRCType, InvertIQ};

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 7, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);
}


void LAMBDA62_SetRx(uint32_t Timeout) {
	while(LAMBDA62_CheckBusy()) {}

	HAL_GPIO_WritePin(L62_TXSW_PORT, L62_TXSW_PIN, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(L62_RXSW_PORT, L62_RXSW_PIN, GPIO_PIN_SET);

	uint8_t tx[4] = {L62_SETRX, (uint8_t)(Timeout >> 16), (uint8_t)(Timeout >> 8), (uint8_t)Timeout};

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);
}


void LAMBDA62_GetRxBufferStatus(uint8_t *len, uint8_t *start) {
	while(LAMBDA62_CheckBusy()) {}

	uint8_t tx[4] = {L62_RX_BUFF_STATUS, 0, 0, 0};   	// Skip status return on 2nd byte
	uint8_t rx[4] = {0};

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(&hspi3_rf, tx, rx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	*len = rx[2];
	*start = rx[3];
}


void LAMBDA62_ReadBuffer(uint8_t *buff, uint8_t StartAddr, uint8_t len) {
	while(LAMBDA62_CheckBusy()) {}

	// Send opcode with offset then read into buffer
	uint8_t tx[3] = {L62_READ_BUFF, StartAddr, 0};

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 3, HAL_MAX_DELAY);
	HAL_SPI_Receive(&hspi3_rf, buff, len, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);
}


uint8_t LAMBDA62_ReadReg(uint16_t RegAddr) {
	while(LAMBDA62_CheckBusy()) {}

	uint8_t tx[5] = {0};   	// Skip status return on 4th byte
	uint8_t rx[5] = {0};

	tx[0] = L62_READ_REG;
	tx[1] = (uint8_t)(RegAddr >> 8);
	tx[2] = (uint8_t)RegAddr;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(&hspi3_rf, tx, rx, 5, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	return rx[4];
}


void LAMBDA62_WriteReg(uint16_t RegAddr, uint8_t val) {
	while(LAMBDA62_CheckBusy()) {}

	uint8_t tx[4];

	tx[0] = L62_WRITE_REG;
	tx[1] = (uint8_t)(RegAddr >> 8);
	tx[2] = (uint8_t)RegAddr;
	tx[3] = val;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);
}


void LAMBDA62_SendContinuousWave() {
	while(LAMBDA62_CheckBusy()) {}

	uint8_t tx = L62_SETCW;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, &tx, 1, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);
}


