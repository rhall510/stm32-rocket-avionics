#include "lsm6dsr.h"
#include "stm32g4xx.h"


extern SPI_HandleTypeDef hspi1_acc;


void InitialiseLSM6DSR(uint16_t WatermarkReads) {
	uint16_t WatermarkWords = WatermarkReads * LSM6_FIFO_DATA_BLOCK_SIZE;

	// Software reset
	uint8_t tx[2] = {LSM6_CTRL3_C, 0b00000001U};

	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);

	HAL_Delay(5);

	// Disable I3C
	tx[0] = LSM6_CTRL9_XL;
	tx[1] = 0b11100010U;

	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);

	// Disable I2C
	tx[0] = LSM6_CTRL4_C;
	tx[1] = 0b00000100U;

	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);

	// Wake up accelerometer
	tx[0] = LSM6_CTRL1_XL;
	tx[1] = 0b01000100U;   // 104Hz, +/-16g, first stage filtering

	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);

	// Wake up gyroscope
	tx[0] = LSM6_CTRL2_G;
	tx[1] = 0b01001100U;   // 104Hz, +/-2000dps

	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);

	// Set up FIFO
	tx[0] = LSM6_FIFO_CTRL4;
	tx[1] = 0b01000110U;   // FIFO continuous mode with full timestamp writing

	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);

	// Set Acc/Gyr write rates
	tx[0] = LSM6_FIFO_CTRL3;
	tx[1] = 0b01000100U;   // 104Hz

	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);

	// Enable timestamp
	tx[0] = LSM6_CTRL10_C;
	tx[1] = 0b00100000U;

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


void LSM6DSR_ReadFIFOData(volatile struct TS_Vec3 *accbuff, volatile struct TS_Vec3 *gyrbuff, uint16_t readnum, float readytime) {
	uint16_t words = readnum * LSM6_FIFO_DATA_BLOCK_SIZE;
	float starttime = 0.0f;

	for (int word = 0; word < words; word++) {
		// Request data from the 7 contiguous FIFO registers
		uint8_t tx[8] = {LSM6_FIFO_DATA_OUT | (1U << 7), 0, 0, 0, 0, 0, 0, 0};
		uint8_t rx[8] = {0};

		HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_RESET);
		HAL_SPI_TransmitReceive(&hspi1_acc, tx, rx, 8, HAL_MAX_DELAY);
		HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);

		// Decode tag byte
		uint8_t type = (rx[1] & 0b11111000U) >> 3;

		int block = (int)(word / LSM6_FIFO_DATA_BLOCK_SIZE);

		if (type == 0x1U) {   // Gyroscope word
			// Combine into raw 16-bit readings
			int16_t gx_raw = (int16_t)((rx[3] << 8) | rx[2]);
			int16_t gy_raw = (int16_t)((rx[5] << 8) | rx[4]);
			int16_t gz_raw = (int16_t)((rx[7] << 8) | rx[6]);

			// Convert to dps
			gyrbuff[block].X = (gx_raw * 70.0f) / 1000.0f;
			gyrbuff[block].Y = (gy_raw * 70.0f) / 1000.0f;
			gyrbuff[block].Z = (gz_raw * 70.0f) / 1000.0f;
		} else if (type == 0x2U) {   // Accelerometer word
			// Combine into raw 16-bit readings
			int16_t ax_raw = (int16_t)((rx[3] << 8) | rx[2]);
			int16_t ay_raw = (int16_t)((rx[5] << 8) | rx[4]);
			int16_t az_raw = (int16_t)((rx[7] << 8) | rx[6]);

			// Convert to gs
			accbuff[block].X = (ax_raw * 0.488f) / 1000.0f;
			accbuff[block].Y = (ay_raw * 0.488f) / 1000.0f;
			accbuff[block].Z = (az_raw * 0.488f) / 1000.0f;
		} else if (type == 0x4U) {   // Timestamp word
			uint32_t ts_raw = (uint32_t)((rx[5] << 24) | (rx[4] << 16) | (rx[3] << 8) | rx[2]);
			float ts = ts_raw * 0.000025f;   // 25us per LSB

			if (block == 0) { starttime = ts; }

			accbuff[block].Timestamp = ts - starttime + readytime;
			gyrbuff[block].Timestamp = ts - starttime + readytime;
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


bool LSM6DSR_AppendLogPacket(uint8_t *buff, uint16_t *BuffPos, uint16_t BuffMaxLen, volatile struct TS_Vec3 *accbuff, volatile struct TS_Vec3 *gyrbuff, uint8_t Readings) {
	// Check the data can fit in the buffer
	uint16_t ByteLen = Readings * 2 * LSM6_PKT_DATA_LEN;

	if (BuffMaxLen - *BuffPos < ByteLen) {
		printf("WARNING: Could not write LSM6DSR data to write buffer due to lack of space");
		return false;
	}

	// Copy data into the buffer
	for (int i = 0; i < Readings; i++) {
		buff[*BuffPos] = 0b00000000;   // Type 0 packet (low range acc)
		*BuffPos += 1;
		buff[*BuffPos] = 4 * sizeof(float);
		*BuffPos += 1;

		memcpy(buff + *BuffPos, &accbuff[i].Timestamp, sizeof(float));
		*BuffPos += sizeof(float);
		memcpy(buff + *BuffPos, &accbuff[i].X, sizeof(float));
		*BuffPos += sizeof(float);
		memcpy(buff + *BuffPos, &accbuff[i].Y, sizeof(float));
		*BuffPos += sizeof(float);
		memcpy(buff + *BuffPos, &accbuff[i].Z, sizeof(float));
		*BuffPos += sizeof(float);

		buff[*BuffPos] = 0b00010000;   // Type 1 packet (gyroscope)
		*BuffPos += 1;
		buff[*BuffPos] = 4 * sizeof(float);
		*BuffPos += 1;

		memcpy(buff + *BuffPos, &gyrbuff[i].Timestamp, sizeof(float));
		*BuffPos += sizeof(float);
		memcpy(buff + *BuffPos, &gyrbuff[i].X, sizeof(float));
		*BuffPos += sizeof(float);
		memcpy(buff + *BuffPos, &gyrbuff[i].Y, sizeof(float));
		*BuffPos += sizeof(float);
		memcpy(buff + *BuffPos, &gyrbuff[i].Z, sizeof(float));
		*BuffPos += sizeof(float);
	}

	return true;
}




