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


	// Set frequency to 868MHz
	tx[0] = L62_RF_FREQ;
	tx[1] = 0x36;
	tx[2] = 0x40;
	tx[3] = 0x00;
	tx[4] = 0x00;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 5, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);


	// Set PA config (DutyCycle, hpMax, deviceSel, paLut)
	tx[0] = L62_PA_CFG;
	tx[1] = 0x04;
	tx[2] = 0x07;
	tx[3] = 0x00;
	tx[4] = 0x01;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 5, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);


	// Set TX params (Power, RampTime)
	tx[0] = L62_TX_PARAMS;
	tx[1] = 0x16;
	tx[2] = 0x04;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 3, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);


	// Set buffer base addresses (TX, RX)
	tx[0] = L62_BUFF_BASE_ADDR;
	tx[1] = 0x0;
	tx[2] = 0x8;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 3, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);


	// Set modulation params (SF, BW, CR, LDR_OP)
	tx[0] = L62_MOD_PARAMS;
	tx[1] = 0x07;
	tx[2] = 0x04;
	tx[3] = 0x01;
	tx[4] = 0x00;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 5, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);


	// Set Tx done interrupt on DIO1 and Rx done interrupt on DIO2
	uint8_t irq[9] = {L62_SET_IRQ, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00};

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, irq, 9, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);
}


void LAMBDA62_SetTx(uint32_t Timeout) {
	HAL_GPIO_WritePin(L62_RXSW_PORT, L62_RXSW_PIN, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(L62_TXSW_PORT, L62_TXSW_PIN, GPIO_PIN_SET);

	uint8_t tx[4] = {L62_SETTX, (uint8_t)(Timeout >> 16), (uint8_t)(Timeout >> 8), (uint8_t)Timeout};

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);
}


void LAMBDA62_SetRx(uint32_t Timeout) {
	HAL_GPIO_WritePin(L62_TXSW_PORT, L62_TXSW_PIN, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(L62_RXSW_PORT, L62_RXSW_PIN, GPIO_PIN_SET);

	uint8_t tx[4] = {L62_SETRX, (uint8_t)(Timeout >> 16), (uint8_t)(Timeout >> 8), (uint8_t)Timeout};

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi3_rf, tx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);
}
