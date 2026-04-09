#include "lsm6dsr.h"
#include "stm32g4xx.h"


extern SPI_HandleTypeDef hspi1_acc;


void InitialiseLSM6DSR(uint16_t WatermarkWords) {
	// Software reset
	uint8_t tx[2] = {LSM6_CTRL3_C, 0b00000001U};

	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);

	HAL_Delay(5);

	// Wake up accelerometer
	tx[0] = LSM6_CTRL1_XL;
//	tx[1] = 0b01000100U;   // 104Hz, +/-16g, first stage filtering
	tx[1] = 0b00010100U;   // 12.5Hz, +/-16g, first stage filtering

	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);

	// Wake up gyroscope
	tx[0] = LSM6_CTRL2_G;
//	tx[1] = 0b01001100U;   // 104Hz, +/-200dps
	tx[1] = 0b00011100U;   // 12.5Hz, +/-200dps

	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);

	// Set up FIFO
	tx[0] = LSM6_FIFO_CTRL4;
	tx[1] = 0b00000110U;   // FIFO continuous mode (new data overwrites old when full)

	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);

	// Set Acc/Gyr write rates
	tx[0] = LSM6_FIFO_CTRL3;
//	tx[1] = 0b01000100U;   // 104Hz
	tx[1] = 0b00010001U;   // 12.5Hz

	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);

	// Configure watermark
	uint8_t tx2[3] = {LSM6_FIFO_CTRL1, (uint8_t)WatermarkWords, (uint8_t)((WatermarkWords >> 8) & 1U)};

	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx2, 3, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);

	// Configure INT1 watermark trigger
	tx[0] = LSM6_INT1_CTRL;
	tx[1] = 0b00001000U;

	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);



//	uint8_t tx[2] = {LSM6_FIFO_CTRL4, 0x00U};   // Completely disable FIFO
//
//	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_RESET);
//	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
//	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);
}


struct Vector3 LSM6DSR_ReadInstAccelData() {
	// Request data from the 6 contiguous accelerometer registers
	uint8_t tx[7] = {LSM6_OUT_A | (1U << 7), 0, 0, 0, 0, 0 ,0};
	uint8_t rx[7] = {0};

	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(&hspi1_acc, tx, rx, 7, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);

	// Combine into raw 16-bit readings
	int16_t ax_raw = (int16_t)((rx[2] << 8) | rx[1]);
	int16_t ay_raw = (int16_t)((rx[4] << 8) | rx[3]);
	int16_t az_raw = (int16_t)((rx[6] << 8) | rx[5]);

	// Convert to gs
	struct Vector3 outvec;

	outvec.X = (ax_raw * 0.488f) / 1000.0f;
	outvec.Y = (ay_raw * 0.488f) / 1000.0f;
	outvec.Z = (az_raw * 0.488f) / 1000.0f;

	return outvec;
}


struct Vector3 LSM6DSR_ReadInstGyroData() {
	// Request data from the 6 contiguous accelerometer registers
	uint8_t tx[7] = {LSM6_OUT_G | (1U << 7), 0, 0, 0, 0, 0 ,0};
	uint8_t rx[7] = {0};

	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(&hspi1_acc, tx, rx, 7, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);

	// Combine into raw 16-bit readings
	int16_t gx_raw = (int16_t)((rx[2] << 8) | rx[1]);
	int16_t gy_raw = (int16_t)((rx[4] << 8) | rx[3]);
	int16_t gz_raw = (int16_t)((rx[6] << 8) | rx[5]);

	// Convert to dps
	struct Vector3 outvec;

	outvec.X = (gx_raw * 70.0f) / 1000.0f;
	outvec.Y = (gy_raw * 70.0f) / 1000.0f;
	outvec.Z = (gz_raw * 70.0f) / 1000.0f;

	return outvec;
}


void LSM6DSR_ReadFIFOData(volatile uint8_t (*buff)[7], uint16_t words) {
	for (int word = 0; word < words; word++) {
		// Request data from the 7 contiguous FIFO registers
		uint8_t tx[8] = {LSM6_FIFO_DATA_OUT | (1U << 7), 0, 0, 0, 0, 0, 0, 0};
		uint8_t rx[8] = {0};

		HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_RESET);
		HAL_SPI_TransmitReceive(&hspi1_acc, tx, rx, 8, HAL_MAX_DELAY);
		HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);

		for (int i = 0; i < 7; i++) {
			buff[word][i] = rx[i + 1];
		}
	}
}



uint16_t LSM6DSR_GetFIFOStatus() {
	// Request data from the 2 contiguous FIFO status registers
	uint8_t tx[3] = {LSM6_FIFO_STATUS | (1U << 7), 0, 0};
	uint8_t rx[3] = {0};

	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(&hspi1_acc, tx, rx, 3, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);

	return (uint16_t)((rx[2] << 8) | rx[1]);
}


