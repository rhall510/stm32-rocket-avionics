#include "adxl375.h"
#include "stm32g4xx.h"


extern SPI_HandleTypeDef hspi1_acc;


void InitialiseADXL375(uint8_t WatermarkWords) {
	// Set to standby mode
	uint8_t tx[2] = {ADXL_POWER_CTL, 0b00000000U};

	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_SET);

	HAL_Delay(5);

	// Clear FIFO by putting it in bypass mode
	tx[0] = ADXL_FIFO_CTL;
	tx[1] = 0b00000000U;

	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_SET);


	// Set output data rate
	tx[0] = ADXL_BW_RATE;
	tx[1] = 0b00001010U;   // 100Hz

	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_SET);


	// Enable watermark interrupt in INT1
	tx[0] = ADXL_INT_ENABLE;
	tx[1] = 0b00000000U;   // Disable all interrupts

	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_SET);

	tx[0] = ADXL_INT_MAP;
	tx[1] = 0b00000000U;   // Map all interrupts to INT1

	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_SET);

	tx[0] = ADXL_INT_ENABLE;
	tx[1] = 0b00000010U;   // Enable watermark interrupt only

	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_SET);


	// Re-enable FIFO
	tx[0] = ADXL_FIFO_CTL;
	tx[1] = 0b10000000U | WatermarkWords;   // Streaming mode, link unused trigger to INT1, set watermark level

	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_SET);


	// Set to measurement mode
	tx[0] = ADXL_POWER_CTL;
	tx[1] = 0b00001000U;

	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi1_acc, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_SET);
}



struct Vector3 ADXL375_ReadSingleAccelData() {
	// Request data from the 6 contiguous accelerometer registers
	uint8_t tx[7] = {ADXL_DATA | (3U << 6), 0, 0, 0, 0, 0 ,0};   // Set first bit for read, second bit for sequential addresses
	uint8_t rx[7] = {0};

	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(&hspi1_acc, tx, rx, 7, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_SET);

	// Combine into raw 16-bit readings
	int16_t ax_raw = (int16_t)((rx[2] << 8) | rx[1]);
	int16_t ay_raw = (int16_t)((rx[4] << 8) | rx[3]);
	int16_t az_raw = (int16_t)((rx[6] << 8) | rx[5]);

	// Convert to gs
	struct Vector3 outvec;

	outvec.X = ax_raw * 0.049f;   // 49mg/LSB
	outvec.Y = ay_raw * 0.049f;
	outvec.Z = az_raw * 0.049f;

	return outvec;
}


void ADXL375_ReadFIFOData(volatile struct TS_Vec3 *accbuff, uint8_t readnum, float readytime) {
	for (int word = 0; word < readnum; word++) {
		struct Vector3 acc = ADXL375_ReadSingleAccelData();
		struct TS_Vec3 out;
		out.X = acc.X;
		out.Y = acc.Y;
		out.Z = acc.Z;
		out.Timestamp = readytime - (readnum - word - 1) * (1.0f / ADXL_DR_FREQ);   // Interpolate timestamp based on data rate

		accbuff[word] = out;

		for(volatile int i = 0; i < 1500; i++) {
			__NOP();
		}
	}
}


uint8_t ADXL375_GetFIFOStatus() {
	uint8_t tx[2] = {ADXL_FIFO_STATUS | (1U << 7), 0};
	uint8_t rx[2] = {0};

	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(&hspi1_acc, tx, rx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_SET);

	return rx[1];
}



bool ADXL375_AppendLogPacket(uint8_t *buff, uint16_t *BuffPos, uint16_t BuffMaxLen, volatile struct TS_Vec3 *databuff, uint8_t Readings) {
	// Check the data can fit in the buffer
	uint16_t ByteLen = Readings * ADXL_PKT_DATA_LEN;

	if (BuffMaxLen - *BuffPos < ByteLen) {
		return false;
	}

	// Copy data into the buffer
	for (int i = 0; i < Readings; i++) {
		buff[*BuffPos] = 0b00100000;   // Type 2 packet (high range acc)
		*BuffPos += 1;
		buff[*BuffPos] = 4 * sizeof(float);
		*BuffPos += 1;

		memcpy(buff + *BuffPos, &databuff[i].Timestamp, sizeof(float));
		*BuffPos += sizeof(float);
		memcpy(buff + *BuffPos, &databuff[i].X, sizeof(float));
		*BuffPos += sizeof(float);
		memcpy(buff + *BuffPos, &databuff[i].Y, sizeof(float));
		*BuffPos += sizeof(float);
		memcpy(buff + *BuffPos, &databuff[i].Z, sizeof(float));
		*BuffPos += sizeof(float);
	}

	return true;
}


















