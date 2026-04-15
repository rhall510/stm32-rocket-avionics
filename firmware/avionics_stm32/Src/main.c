#include "main.h"
#include <stdio.h>


volatile struct TS_Vec3 lsm_accbuff[LSM6_FIFO_READNUM];
volatile struct TS_Vec3 lsm_gyrbuff[LSM6_FIFO_READNUM];
volatile bool lsm_data_ready = false;
volatile float lsm_data_time = 0.0f;

volatile struct TS_Vec3 adxl_accbuff[ADXL_FIFO_READNUM];
volatile bool adxl_data_ready = false;
volatile float adxl_data_time = 0.0f;

volatile struct TS_PressTemp bmp_buff[BMP_FIFO_READNUM];
volatile bool bmp_data_ready = false;
volatile float bmp_data_time = 0.0f;

float press = 0.0f;
float temp = 0.0f;


int main(void) {
	HAL_Init();

	SystemClockConfig();
	InitialiseGPIO();
	InitialiseI2C();
	InitialiseSPI();

	__enable_irq();

	InitialiseLSM6DSR(LSM6_FIFO_READNUM);
	InitialiseADXL375(ADXL_FIFO_READNUM);
	if (!InitialiseBMP581(BMP_FIFO_READNUM)) {
		Error_Handler();
	}


//	HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(&hi2c, mmc_address, reg_addr, I2C_MEMADD_SIZE_8BIT, &product_id, 1, 100);
//
//	if (ret == HAL_OK) {
//	    if (product_id == 0x30) {
//	        printf("SUCCESS: MMC5983MA is ALIVE! Product ID: 0x%02X\n", product_id);
//	    } else {
//	        printf("WARNING: Device at 0x30 responded, but gave wrong ID: 0x%02X\n", product_id);
//	    }
//	} else {
//	    // Print the HAL error code (0=OK, 1=ERROR, 2=BUSY, 3=TIMEOUT)
//	    printf("FAILURE: Dead silence. HAL Status Code: %d\n", ret);
//	}





	while (1) {
		if (lsm_data_ready) {
			lsm_data_ready = false;
			LSM6DSR_ReadFIFOData(lsm_accbuff, lsm_gyrbuff, LSM6_FIFO_READNUM, lsm_data_time);
		}
		if (adxl_data_ready) {
			adxl_data_ready = false;
			ADXL375_ReadFIFOData(adxl_accbuff, ADXL_FIFO_READNUM, adxl_data_time);
		}
		if (bmp_data_ready) {
			bmp_data_ready = false;
			BMP581_ReadFIFOData(bmp_buff, BMP_FIFO_READNUM, bmp_data_time);

			press = bmp_buff[0].Press;
			temp = bmp_buff[0].Temp;

			ULED_TOGGLE
		}

		HAL_Delay(10);
	}

	for(;;);
}



// Initialisation functions
void SystemClockConfig(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    // Configure main internal regulator output voltage
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

    // Initialise the RCC Oscillators
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
    RCC_OscInitStruct.PLL.PLLN = 85;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

    // Initialise CPU, AHB and APB bus clocks
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) { Error_Handler(); }
}

void InitialiseI2C() {
	// Set I2C clock source
	RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C2;
    PeriphClkInit.I2c2ClockSelection = RCC_I2C2CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) { Error_Handler(); }

	// Turn on port A clock
	__HAL_RCC_GPIOA_CLK_ENABLE();

	// Configure alternate function pins
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	// Turn on I2C clock
	__HAL_RCC_I2C2_CLK_ENABLE();

	// Initialise I2C2 (magnetometer, barometer, GPS)
	hi2c.Instance = I2C2;
	hi2c.Init.Timing = 0x40B285C2;   // PRESC (0), SCLDEL (1-2), SDADEL (3), SCLH (4-5), SCLL (6-7)
	hi2c.Init.OwnAddress1 = 0;
	hi2c.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;   // Standard 7-bit addressing
	hi2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;   // Disable dual addressing (reading two slaves simultaneously)
	hi2c.Init.OwnAddress2 = 0;
	hi2c.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
	hi2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;   // Disable ability to respond to a broadcast
	hi2c.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;   // Allow slaves to hold the clock low to pause communication

	if (HAL_I2C_Init(&hi2c) != HAL_OK) { Error_Handler(); }

	// Turn on analogue filter
	if (HAL_I2CEx_ConfigAnalogFilter(&hi2c, I2C_ANALOGFILTER_ENABLE) != HAL_OK) { Error_Handler(); }

	// Turn off digital filter
	if (HAL_I2CEx_ConfigDigitalFilter(&hi2c, 0) != HAL_OK) { Error_Handler(); }
}

void InitialiseSPI() {
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	// Turn on clocks
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_SPI3_CLK_ENABLE();

	// Initialise SPI1 (accelerometers)
    GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	hspi1_acc.Instance = SPI1;
	hspi1_acc.Init.Mode = SPI_MODE_MASTER;
	hspi1_acc.Init.Direction = SPI_DIRECTION_2LINES;   // Standard full-duplex SPI
	hspi1_acc.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi1_acc.Init.CLKPolarity = SPI_POLARITY_HIGH;   // Clock is high when idle
	hspi1_acc.Init.CLKPhase = SPI_PHASE_2EDGE;   // Data is read on the rising edge of the clock
	hspi1_acc.Init.NSS = SPI_NSS_SOFT;   // CS pins must be manually controlled
	hspi1_acc.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;   // Must be low enough for ADXL 5MHz limit
	hspi1_acc.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi1_acc.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi1_acc.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;   // Disable CRC check to save processing time
	hspi1_acc.Init.CRCPolynomial = 7;
	hspi1_acc.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
	hspi1_acc.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;   // Enable CS pin pulse to reset logic between frames

	if (HAL_SPI_Init(&hspi1_acc) != HAL_OK) { Error_Handler(); }


	// Initialise SPI2 (storage)
    GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	hspi2_str.Instance = SPI2;
	hspi2_str.Init.Mode = SPI_MODE_MASTER;
	hspi2_str.Init.Direction = SPI_DIRECTION_2LINES;
	hspi2_str.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi2_str.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi2_str.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi2_str.Init.NSS = SPI_NSS_SOFT;
	hspi2_str.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
	hspi2_str.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi2_str.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi2_str.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi2_str.Init.CRCPolynomial = 7;
	hspi2_str.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
	hspi2_str.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;

	if (HAL_SPI_Init(&hspi2_str) != HAL_OK) { Error_Handler(); }


	// Initialise SPI3 (RF communication)
    GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	hspi3_rf.Instance = SPI3;
	hspi3_rf.Init.Mode = SPI_MODE_MASTER;
	hspi3_rf.Init.Direction = SPI_DIRECTION_2LINES;
	hspi3_rf.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi3_rf.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi3_rf.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi3_rf.Init.NSS = SPI_NSS_SOFT;
	hspi3_rf.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
	hspi3_rf.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi3_rf.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi3_rf.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi3_rf.Init.CRCPolynomial = 7;
	hspi3_rf.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
	hspi3_rf.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;

	if (HAL_SPI_Init(&hspi3_rf) != HAL_OK) { Error_Handler(); }
}

void InitialiseGPIO() {
	// Enable all port clocks
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOF_CLK_ENABLE();

	// Enable SYSCFG clock for interrupts
	__HAL_RCC_SYSCFG_CLK_ENABLE();

	GPIO_InitTypeDef GPIO_InitStruct = {0};

	// ULED
	HAL_GPIO_WritePin(ULED_PORT, ULED_PIN, GPIO_PIN_RESET);
	GPIO_InitStruct.Pin = ULED_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(ULED_PORT, &GPIO_InitStruct);


	// UBUTTON
	GPIO_InitStruct.Pin = UBTN_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(UBTN_PORT, &GPIO_InitStruct);


	// LSM6DSR pins
	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);   // Default SPI CS pins to high
	GPIO_InitStruct.Pin = LSM6_CS_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(LSM6_CS_PORT, &GPIO_InitStruct);


	GPIO_InitStruct.Pin = LSM6_INT1_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(LSM6_INT1_PORT, &GPIO_InitStruct);

	HAL_NVIC_SetPriority(EXTI4_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(EXTI4_IRQn);


	GPIO_InitStruct.Pin = LSM6_INT2_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(LSM6_INT2_PORT, &GPIO_InitStruct);

	HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);


	// ADXL375 pins
	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_SET);   // Default SPI CS pins to high
	GPIO_InitStruct.Pin = ADXL_CS_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(ADXL_CS_PORT, &GPIO_InitStruct);


	GPIO_InitStruct.Pin = ADXL_INT1_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(ADXL_INT1_PORT, &GPIO_InitStruct);

	HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(EXTI0_IRQn);


	GPIO_InitStruct.Pin = ADXL_INT2_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(ADXL_INT2_PORT, &GPIO_InitStruct);

	HAL_NVIC_SetPriority(EXTI1_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(EXTI1_IRQn);


	// BMP581 pins
	GPIO_InitStruct.Pin = BMP_INT_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(BMP_INT_PORT, &GPIO_InitStruct);

	HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}



// External interrupt handlers
void EXTI0_IRQHandler(void) {
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_0);
	adxl_data_ready = true;
	adxl_data_time = (float)uwTick / 1000.0f;
}

void EXTI1_IRQHandler(void) {
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_1);
}

void EXTI2_IRQHandler(void) {
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_2);
}

void EXTI3_IRQHandler(void) {
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_3);
}

void EXTI4_IRQHandler(void) {
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_4);
	lsm_data_ready = true;
	lsm_data_time = (float)uwTick / 1000.0f;
}

void EXTI9_5_IRQHandler(void) {
	if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_5) != RESET) {
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_5);
	} if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_6) != RESET) {
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_6);
	} if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_7) != RESET) {
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_7);
	} if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_8) != RESET) {
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_8);
	} if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_9) != RESET) {
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_9);
	}
}

void EXTI15_10_IRQHandler(void) {
	if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_10) != RESET) {
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_10);
	} if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_11) != RESET) {
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_11);
		bmp_data_ready = true;
		bmp_data_time = (float)uwTick / 1000.0f;
	} if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_12) != RESET) {
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_12);
	} if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_13) != RESET) {
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_13);
	} if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_14) != RESET) {
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_14);
	} if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_15) != RESET) {
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_15);
	}
}

// Error handlers
void NMI_Handler(void) {
    __disable_irq();
    while (1) {}
}

void HardFault_Handler(void) {
    __disable_irq();
    while (1) {}
}

void MemManage_Handler(void) {
    __disable_irq();
    while (1) {}
}

void BusFault_Handler(void) {
    __disable_irq();
    while (1) {};
}

void UsageFault_Handler(void) {
    __disable_irq();
    while (1) {}
}

void SVC_Handler(void) {}

void DebugMon_Handler(void) {}

void PendSV_Handler(void) {}

void SysTick_Handler(void) {
	HAL_IncTick();
}



// General error handler
void Error_Handler(void) {
    __disable_irq();
    while (1) {}
}
