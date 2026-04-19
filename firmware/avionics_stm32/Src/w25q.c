#include "w25q.h"
#include "stm32g4xx.h"


extern SPI_HandleTypeDef hspi2_str;


void W25Q_Reset() {
	// Enable software reset
	uint8_t tx[1] = {W25Q_ENABLE_RESET};

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2_str, tx, 1, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	// Execute software reset
	tx[0] = W25Q_RESET;

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2_str, tx, 1, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	HAL_Delay(1);   // Must wait at least 30uS
}




bool InitialiseW25Q(bool Erase) {
	// Check device ID
	uint8_t tx[4] = {0};
	uint8_t rx[4] = {0};

	tx[0] = W25Q_JEDECID;

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(&hspi2_str, tx, rx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	if (rx[1] != 0xEF || (((uint16_t)rx[2] << 8) + rx[3]) != 0x4019) {
		return false;
	}


	// Erase entire chip if chosen
	if (Erase) {
		W25Q_EnableWrite();
		W25Q_EraseChip();

		FirstFreeAddr = 0;
		FirstFreePagePosition = 0;
	} else {
		FirstFreeAddr = W25Q_GetFirstFreePosition();
		FirstFreePagePosition = LastAddr % 256;
	}


	// Set to 4 byte address mode
	tx[0] = W25Q_4BYTE_ADDR_MODE;

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2_str, tx, 1, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	return true;
}


bool W25Q_CheckBusy() {
	// Read status 1 register
	uint8_t tx[2] = {W25Q_READ_STATUS1, 0};
	uint8_t rx[2] = {0};

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(&hspi2_str, tx, rx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	return rx[1] & 1;
}


void W25Q_EnableWrite() {
	uint8_t tx[1] = {W25Q_WRITE_ENABLE};

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2_str, tx, 1, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);
}



uint32_t W25Q_GetFirstFreePosition() {

}






