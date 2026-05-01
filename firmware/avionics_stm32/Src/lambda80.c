#include "lambda80.h"
#include "stm32g4xx.h"


extern SPI_HandleTypeDef hspi3_rf;


void InitialiseLAMBDA80() {
	uint8_t tx[5] = {0};

	// Hardware reset
	HAL_GPIO_WritePin(L80_RST_PORT, L80_RST_PIN, GPIO_PIN_RESET);
	HAL_Delay(1);
	HAL_GPIO_WritePin(L80_RST_PORT, L80_RST_PIN, GPIO_PIN_SET);

	while (LAMBDA80_CheckBusy()) {}   // Wait for startup

	// Set packet type to LoRa
	tx[0] = L80_PKT_TYPE;
	tx[1] = 0x1;

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

	while(LAMBDA80_CheckBusy()) {}

	// Set frequency to 2.4GHz (values from datasheet)
	tx[0] = L80_RF_FREQ;
	tx[1] = 0xB8;
	tx[2] = 0x9D;
	tx[3] = 0x89;

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

	while(LAMBDA80_CheckBusy()) {}

	// Set TX params (Power, RampTime)
	tx[0] = L80_TX_PARAMS;
	tx[1] = 0x31;
	tx[2] = 0x80;

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 3, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

	while(LAMBDA80_CheckBusy()) {}

	// Set buffer base addresses (TX, RX)
	tx[0] = L80_BUFF_BASE_ADDR;
	tx[1] = L80_TX_BASE_ADDR;
	tx[2] = L80_RX_BASE_ADDR;

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 3, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);


	LAMBDA80_SetMode_Telemetry();   // Default to receiving packets

	// Set Tx done interrupt on DIO1 and Rx done interrupt on DIO2
	uint8_t irq[9] = {L80_SET_IRQ, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00};

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, irq, 9, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

	while(LAMBDA80_CheckBusy()) {}
}


void LAMBDA80_SetMode_Telemetry(){
	while(LAMBDA80_CheckBusy()) {}

	uint8_t tx[5] = {0};

	// Set modulation params (SF, BW, CR)
	tx[0] = L80_MOD_PARAMS;
	tx[1] = 0xA0;   // SF10
	tx[2] = 0x18;   // BW 812.5KHz
	tx[3] = 0x01;   // CR 4/5

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

	while(LAMBDA80_CheckBusy()) {}

	LAMBDA80_WriteReg(0x925, 0x32);   // From datasheet for SF10

	// Set default packet parameters
	LAMBDA80_SetPacketParams(0x0C, 0, 30, 0x20, 0x40);   // 12 bit preamble, explicit header, 30 byte payload, CRC on, normal IQ

	LAMBDA80_SetRx(2, 0);   // Set to Rx continuous mode
}


void LAMBDA80_SetMode_Download() {
	while(LAMBDA80_CheckBusy()) {}

	uint8_t tx[5] = {0};

	// Set modulation params (SF, BW, CR)
	tx[0] = L80_MOD_PARAMS;
	tx[1] = 0x50;   // SF5
	tx[2] = 0x0A;   // BW 1625KHz
	tx[3] = 0x01;   // CR 4/5

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

	while(LAMBDA80_CheckBusy()) {}

	LAMBDA80_WriteReg(0x925, 0x1E);   // From datasheet for SF5

	// Set default packet parameters
	LAMBDA80_SetPacketParams(0x0C, 0, 128, 0x20, 0x40);   // 12 bit preamble, explicit header, 128 byte payload, CRC on, normal IQ

	while(LAMBDA80_CheckBusy()) {}
}



inline bool LAMBDA80_CheckBusy() {
	return HAL_GPIO_ReadPin(L80_BUSY_PORT, L80_BUSY_PIN);
}


void LAMBDA80_ClearIRQ(uint16_t IRQMask) {
	uint8_t tx[3] = {L80_CLEAR_IRQ, (uint8_t)(IRQMask >> 8), (uint8_t)IRQMask};

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 3, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);
}



void LAMBDA80_SetTx(uint8_t TimeBase, uint16_t Timeout) {
	while(LAMBDA80_CheckBusy()) {}

	// Timeout is timebase (see below for mappings) * timeout, 0 = no timeout
	// 0x00 15.625 μs
	// 0x01 62.5 μs
	// 0x02 1 ms
	// 0x03 4 ms
	uint8_t tx[4] = {L80_SETTX, TimeBase, (uint8_t)(Timeout >> 8), (uint8_t)Timeout};

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);
}


void LAMBDA80_SendPacket(uint8_t *packet, uint8_t len) {
	while(LAMBDA80_CheckBusy()) {}

	// Send opcode with offset then packet buffer
	uint8_t tx[2] = {L80_WRITE_BUFF, L80_TX_BASE_ADDR};

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 2, HAL_MAX_DELAY);
	HAL_SPI_Transmit(&hspi3_rf, packet, len, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

	while(LAMBDA80_CheckBusy()) {}

	LAMBDA80_SetTx(0x2, L80_TX_TIMEOUT);
}


void LAMBDA80_SetPacketParams(uint8_t PreambleLen, uint8_t HeaderType, uint8_t len, uint8_t CRCType, uint8_t InvertIQ) {
	while(LAMBDA80_CheckBusy()) {}

	uint8_t tx[8] = {L80_PKT_PARAMS, PreambleLen, HeaderType, len, CRCType, InvertIQ, 0x0, 0x0};   // Last 2 bytes not used for LoRa

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 8, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);
}


void LAMBDA80_SetRx(uint8_t TimeBase, uint16_t Timeout) {
	while(LAMBDA80_CheckBusy()) {}

	// Timeout is timebase (see below for mappings) * timeout, 0 = no timeout, 0xFFFF = continuous mode
	// 0x00 15.625 μs
	// 0x01 62.5 μs
	// 0x02 1 ms
	// 0x03 4 ms
	uint8_t tx[4] = {L80_SETRX, TimeBase, (uint8_t)(Timeout >> 8), (uint8_t)Timeout};

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);
}


void LAMBDA80_GetRxBufferStatus(uint8_t *len, uint8_t *start) {
	while(LAMBDA80_CheckBusy()) {}

	uint8_t tx[4] = {L80_RX_BUFF_STATUS, 0, 0, 0};   	// Skip status return on 2nd byte
	uint8_t rx[4] = {0};

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(&hspi3_rf, tx, rx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

	*len = rx[2];
	*start = rx[3];
}


void LAMBDA80_ReadBuffer(uint8_t *buff, uint8_t StartAddr, uint8_t len) {
	while(LAMBDA80_CheckBusy()) {}

	// Send opcode with offset then read into buffer
	uint8_t tx[3] = {L80_READ_BUFF, StartAddr, 0};

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 3, HAL_MAX_DELAY);
	HAL_SPI_Receive(&hspi3_rf, buff, len, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);
}



uint8_t LAMBDA80_ReadReg(uint16_t RegAddr) {
	while(LAMBDA80_CheckBusy()) {}

	uint8_t tx[5] = {0};   	// Skip status return on 4th byte
	uint8_t rx[5] = {0};

	tx[0] = L80_READ_REG;
	tx[1] = (uint8_t)(RegAddr >> 8);
	tx[2] = (uint8_t)RegAddr;

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(&hspi3_rf, tx, rx, 5, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

	return rx[4];
}


void LAMBDA80_WriteReg(uint16_t RegAddr, uint8_t val) {
	while(LAMBDA80_CheckBusy()) {}

	uint8_t tx[4];

	tx[0] = L80_WRITE_REG;
	tx[1] = (uint8_t)(RegAddr >> 8);
	tx[2] = (uint8_t)RegAddr;
	tx[3] = val;

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);
}




