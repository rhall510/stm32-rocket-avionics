#include "mmc5983.h"
#include "stm32g4xx.h"

extern I2C_HandleTypeDef hi2c;


bool InitialiseMMC5983MA() {
	// Issue software reset
	uint8_t buff[1] = {0b10000000};
	HAL_I2C_Mem_Write(&hi2c, MMC_I2C_ADDR, MMC_CTRL1, I2C_MEMADD_SIZE_8BIT, buff, 1, HAL_MAX_DELAY);

	HAL_Delay(15);

	// Check device ID is correct
	buff[0] = 0;
	HAL_I2C_Mem_Read(&hi2c, MMC_I2C_ADDR, MMC_ID, I2C_MEMADD_SIZE_8BIT, buff, 1, HAL_MAX_DELAY);

	if (buff[0] != 0b00110000) {
		printf("MMC5983MA not responsive");
		return false;
	}


	// Enable data ready interrupt
	buff[0] = 0b00000100;
	HAL_I2C_Mem_Write(&hi2c, MMC_I2C_ADDR, MMC_CTRL0, I2C_MEMADD_SIZE_8BIT, buff, 1, HAL_MAX_DELAY);

	// Enable continuous measurement at 10Hz
	buff[0] = 0b00001010;
	HAL_I2C_Mem_Write(&hi2c, MMC_I2C_ADDR, MMC_CTRL2, I2C_MEMADD_SIZE_8BIT, buff, 1, HAL_MAX_DELAY);

	return true;
}


uint8_t MMC5983MA_GetStatus() {
	uint8_t buff[1] = {0};
	HAL_I2C_Mem_Read(&hi2c, MMC_I2C_ADDR, MMC_STATUS, I2C_MEMADD_SIZE_8BIT, buff, 1, HAL_MAX_DELAY);

	return buff[0];
}


void MMC5983MA_ReadData(volatile struct TS_Vec3 *mout, float readytime) {
	uint8_t buff[7] = {0};
	HAL_I2C_Mem_Read(&hi2c, MMC_I2C_ADDR, MMC_DATA, I2C_MEMADD_SIZE_8BIT, buff, 7, HAL_MAX_DELAY);

	mout->Timestamp = readytime;

	uint32_t raw_X = ((uint32_t)buff[0] << 10) | ((uint32_t)buff[1] << 2) | ((buff[6] >> 6) & 0b11);
	uint32_t raw_Y = ((uint32_t)buff[2] << 10) | ((uint32_t)buff[3] << 2) | ((buff[6] >> 4) & 0b11);
	uint32_t raw_Z = ((uint32_t)buff[4] << 10) | ((uint32_t)buff[5] << 2) | ((buff[6] >> 2) & 0b11);

	// Convert to gauss. Subtract null field and divide by sensitivity
	mout->X = ((float)raw_X - 131072.0f) / 16384.0f;
	mout->Y = ((float)raw_Y - 131072.0f) / 16384.0f;
	mout->Z = ((float)raw_Z - 131072.0f) / 16384.0f;

	// Clear interrupt
	buff[0] = 0b00000011;
	HAL_I2C_Mem_Write(&hi2c, MMC_I2C_ADDR, MMC_STATUS, I2C_MEMADD_SIZE_8BIT, buff, 1, HAL_MAX_DELAY);
}


bool MMC5983MA_AppendLogPacket(uint8_t *buff, uint16_t *BuffPos, uint16_t BuffMaxLen, volatile struct TS_Vec3 *databuff, uint8_t Readings) {
	// Check the data can fit in the buffer
	uint16_t ByteLen = Readings * MMC_PKT_DATA_LEN;

	if (BuffMaxLen - *BuffPos < ByteLen) {
		printf("WARNING: Could not write MMC5983MA data to write buffer due to lack of space");
		return false;
	}

	// Copy data into the buffer
	for (int i = 0; i < Readings; i++) {
		buff[*BuffPos] = 0b00110000;   // Type 3 packet (magnetometer)
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



