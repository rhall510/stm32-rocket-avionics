#include "misc.h"
#include "stm32g4xx.h"
#include <stdio.h>


extern I2C_HandleTypeDef hi2c;


void ScanI2CBus() {
	for (uint8_t i = 0; i < 128; i++) {
		if (HAL_I2C_IsDeviceReady(&hi2c, (uint16_t)(i<<1), 3, 5) == HAL_OK) {
			printf("%2x ", i);
		} else {
			printf("-- ");
		}

		if (i > 0 && (i + 1) % 16 == 0) {
			printf("\n");
		}
	}

	printf("\n");
}


