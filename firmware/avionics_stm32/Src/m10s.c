#include "stm32g4xx.h"
#include "m10s.h"

extern I2C_HandleTypeDef hi2c;


uint16_t MAXM10S_GetAvailableBytes() {
    uint8_t buff[2] = {0};

    if (HAL_I2C_Mem_Read(&hi2c, M10S_I2C_ADDR, M10S_AVAIL, I2C_MEMADD_SIZE_8BIT, buff, 2, HAL_MAX_DELAY) == HAL_OK) {
        return (uint16_t)((buff[0] << 8) | buff[1]);
    }
    return 0;
}


bool MAXM10S_ReadStream(uint8_t *buffer, uint16_t length) {
    if (length == 0) return false;

    if (HAL_I2C_Mem_Read(&hi2c, M10S_I2C_ADDR, M10S_DATA, I2C_MEMADD_SIZE_8BIT, buffer, length, HAL_MAX_DELAY) == HAL_OK) {
        return true;
    }
    return false;
}



bool MAXM10S_SendCommand(uint8_t *cmd, uint16_t length) {
    if (length < 2) return false;   // Requires >= 2 bytes for a command write

    if (HAL_I2C_Master_Transmit(&hi2c, M10S_I2C_ADDR, cmd, length, HAL_MAX_DELAY) == HAL_OK) {
        return true;
    }
    return false;
}




void InitialiseMAXM10S() {
	// Command string generated with u-center
	uint8_t confcmd[] = {
		0xB5, 0x62, 0x06, 0x8A, 0x28, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x21, 0x00, 0x11, 0x20, 0x08, 0x01,
		0x00, 0x21, 0x30, 0x64, 0x00, 0xC9, 0x00, 0x91,
		0x20, 0x00, 0xBF, 0x00, 0x91, 0x20, 0x00, 0xC4,
		0x00, 0x91, 0x20, 0x00, 0xB0, 0x00, 0x91, 0x20,
		0x00, 0x01, 0x00, 0xD0, 0x20, 0x00, 0x7A, 0x97
	};

	MAXM10S_SendCommand(confcmd, sizeof(confcmd));
}


