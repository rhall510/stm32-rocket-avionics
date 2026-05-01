#include "bmp581.h"
#include "stm32g4xx.h"


extern I2C_HandleTypeDef hi2c;


bool InitialiseBMP581(uint8_t Threshold) {
	// Issue soft reset
	uint8_t buff[1] = {0xB6};
	HAL_I2C_Mem_Write(&hi2c, BMP_I2C_ADDR, BMP_CMD, I2C_MEMADD_SIZE_8BIT, buff, 1, HAL_MAX_DELAY);

	HAL_Delay(5);

	// Check device ID is correct
	buff[0] = 0;
	HAL_I2C_Mem_Read(&hi2c, BMP_I2C_ADDR, BMP_ASIC_ID, I2C_MEMADD_SIZE_8BIT, buff, 1, HAL_MAX_DELAY);

	if (buff[0] == 0) {
		printf("BMP581 not responsive");
		return false;
	}

	// Check NVM status
	buff[0] = 0;
	HAL_I2C_Mem_Read(&hi2c, BMP_I2C_ADDR, BMP_STATUS, I2C_MEMADD_SIZE_8BIT, buff, 1, HAL_MAX_DELAY);

	if ((buff[0] & 2U) == 0 || (buff[0] & 4U) != 0) {
		printf("BMP581 NVM startup error");
		return false;
	}

	// Check power on reset interrupt status
	buff[0] = 0;
	HAL_I2C_Mem_Read(&hi2c, BMP_I2C_ADDR, BMP_INT_STATUS, I2C_MEMADD_SIZE_8BIT, buff, 1, HAL_MAX_DELAY);

	if (buff[0] != 0x10) {
		printf("BMP581 INT startup error");
		return false;
	}


	// Set FIFO to continuous mode (overwrite when full) and set threshold level
	buff[0] = 0x0U + Threshold;
	HAL_I2C_Mem_Write(&hi2c, BMP_I2C_ADDR, BMP_FIFO_CONF, I2C_MEMADD_SIZE_8BIT, buff, 1, HAL_MAX_DELAY);

	// Configure FIFO to save temperature and pressure
	buff[0] = 0b11;
	HAL_I2C_Mem_Write(&hi2c, BMP_I2C_ADDR, BMP_FIFO_SEL, I2C_MEMADD_SIZE_8BIT, buff, 1, HAL_MAX_DELAY);

	// Enable FIFO watermark interrupt
	buff[0] = 0b00000100;
	HAL_I2C_Mem_Write(&hi2c, BMP_I2C_ADDR, BMP_INT_SOURCE, I2C_MEMADD_SIZE_8BIT, buff, 1, HAL_MAX_DELAY);

	// Enable interrupts in push-pull latched mode, active high
	buff[0] = 0b00111011;
	HAL_I2C_Mem_Write(&hi2c, BMP_I2C_ADDR, BMP_INT_CONF, I2C_MEMADD_SIZE_8BIT, buff, 1, HAL_MAX_DELAY);

	// Set over sampling rate to 64x pressure 4x temperature to increase resolution
	buff[0] = 0b01110010;
	HAL_I2C_Mem_Write(&hi2c, BMP_I2C_ADDR, BMP_OSR, I2C_MEMADD_SIZE_8BIT, buff, 1, HAL_MAX_DELAY);

	// Set data rate to 10Hz and enter normal mode
	buff[0] = 0b01011101;
	HAL_I2C_Mem_Write(&hi2c, BMP_I2C_ADDR, BMP_ODR, I2C_MEMADD_SIZE_8BIT, buff, 1, HAL_MAX_DELAY);

	return true;
}



uint8_t BMP581_GetFIFOCount() {
	// Return number of frames in FIFO
	uint8_t buff[1] = {0};
	HAL_I2C_Mem_Read(&hi2c, BMP_I2C_ADDR, BMP_FIFO_COUNT, I2C_MEMADD_SIZE_8BIT, buff, 1, HAL_MAX_DELAY);

	return buff[0];
}


void BMP581_ReadFIFOData(volatile struct TS_PressTemp *ptbuff, uint8_t readnum, float readytime) {
	// Read specified number of frames from FIFO
	uint8_t num = readnum * BMP_FIFO_DATA_BLOCK_SIZE;

	uint8_t buff[num];
	HAL_I2C_Mem_Read(&hi2c, BMP_I2C_ADDR, BMP_FIFO_DATA, I2C_MEMADD_SIZE_8BIT, buff, num, HAL_MAX_DELAY);

	for (int word = 0; word < readnum; word++) {
		uint32_t rawtemp = (uint32_t)buff[word * BMP_FIFO_DATA_BLOCK_SIZE + 2] << 16;
		rawtemp	+= (uint32_t)buff[word * BMP_FIFO_DATA_BLOCK_SIZE + 1] << 8;
		rawtemp	+= buff[word * BMP_FIFO_DATA_BLOCK_SIZE];
		ptbuff[word].Temp = (float)rawtemp / 0xFFFF;

		uint32_t rawpres = (uint32_t)buff[word * BMP_FIFO_DATA_BLOCK_SIZE + 5] << 16;
		rawpres += (uint32_t)buff[word * BMP_FIFO_DATA_BLOCK_SIZE + 4] << 8;
		rawpres += buff[word * BMP_FIFO_DATA_BLOCK_SIZE + 3];
		ptbuff[word].Press = (float)rawpres / 0b111111;

		ptbuff[word].Timestamp = readytime - (readnum - word - 1) * (1.0f / BMP_DR_FREQ);   // Interpolate timestamp based on data rate
	}

	// Clear interrupt
	HAL_I2C_Mem_Read(&hi2c, BMP_I2C_ADDR, BMP_INT_STATUS, I2C_MEMADD_SIZE_8BIT, buff, 1, HAL_MAX_DELAY);
}



bool BMP581_AppendLogPacket(uint8_t *buff, uint16_t *BuffPos, uint16_t BuffMaxLen, volatile struct TS_PressTemp *databuff, uint8_t Readings) {
	// Check the data can fit in the buffer
	uint16_t ByteLen = Readings * BMP_PKT_DATA_LEN;

	if (BuffMaxLen - *BuffPos < ByteLen) {
		return false;
	}

	// Copy data into the buffer
	for (int i = 0; i < Readings; i++) {
		buff[*BuffPos] = 0b01000000;      // Type 4 packet (press/temp)
		*BuffPos += 1;
		buff[*BuffPos] = 3 * sizeof(float);
		*BuffPos += 1;

		memcpy(buff + *BuffPos, &databuff[i].Timestamp, sizeof(float));
		*BuffPos += sizeof(float);
		memcpy(buff + *BuffPos, &databuff[i].Press, sizeof(float));
		*BuffPos += sizeof(float);
		memcpy(buff + *BuffPos, &databuff[i].Temp, sizeof(float));
		*BuffPos += sizeof(float);
	}

	return true;
}


