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

volatile struct TS_Vec3 mmc_buff;
volatile bool mmc_data_ready = false;
volatile float mmc_data_time = 0.0f;

volatile struct TS_GPS m10s_data = {0};
volatile bool m10s_data_ready = false;
volatile float m10s_data_time = 0.0f;
static char line_buffer[M10S_LINE_BUFFER_SIZE];
static uint8_t line_len = 0;


int main(void) {
	HAL_Init();

	SystemClockConfig();
	InitialiseGPIO();
	InitialiseI2C();
	InitialiseSPI();

	HAL_Delay(2000);


//	Test_W25Q_Logging();
	Test_W25Q_BadBlocks();
	while (1) {
		HAL_Delay(1000);
	}

	__enable_irq();


	ScanI2CBus();


	InitialiseLSM6DSR(LSM6_FIFO_READNUM);
	InitialiseADXL375(ADXL_FIFO_READNUM);
	if (!InitialiseBMP581(BMP_FIFO_READNUM)) {
		Error_Handler();
	}
	if (!InitialiseMMC5983MA()) {
		Error_Handler();
	}
	InitialiseMAXM10S();


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
		}
		if (mmc_data_ready) {
			mmc_data_ready = false;
			MMC5983MA_ReadData(&mmc_buff, mmc_data_time);
		}

		Poll_MAXM10S();

		HAL_Delay(10);
	}

	for(;;);
}



void Test_W25Q_Logging() {
    printf("Starting W25Q test\n");

//    for (int i = 0; i < 10; i++) {
//    	W25Q_EraseBlock(0x10000 * i);
//    }

    // Check initialization + JEDEC ID
    if (!InitialiseW25Q()) {
        printf("W25Q initialization failed\n");
        return;
    }
    printf("W25Q initialized - JEDEC ID verified\n");

    printf("MetaStartAddr is: 0x%08lX\n", MetaStartAddr);

    // Erase existing flight data
    printf("Erasing old flight data...\n");
    W25Q_EraseFlightData();
    printf("Flight data erased. DataStartAddr now: 0x%08lX\n", DataStartAddr);
    printf("MetaStartAddr is now: 0x%08lX\n", MetaStartAddr);

    // Write test packet
    uint8_t testPacket[16] = {0xAA, 0xBB, 0xBB, 0xAA, 0x01, 0x00, 0x10, 0x00, 0x10, 0x06, 'R', 'O', 'C', 'K', 'E', 'T'};

    printf("Writing 16 byte test packet...\n");
    W25Q_WriteAppendData(testPacket, sizeof(testPacket));

    // Read back
    printf("Reading back data from address 0x%08lX...\n", DataStartAddr);
    uint8_t readBuffer[16] = {0};
    W25Q_ReadVolumeSafe(DataStartAddr, readBuffer, sizeof(readBuffer));

    if (memcmp(testPacket, readBuffer, sizeof(testPacket)) == 0) {
        printf("Readback matched the written packet exactly\n");
    } else {
        printf("Readback mismatch\n");
        printf("Expected sync word: %02X %02X %02X %02X\n", testPacket[0], testPacket[1], testPacket[2], testPacket[3]);
        printf("Actual sync word  : %02X %02X %02X %02X\n", readBuffer[0], readBuffer[1], readBuffer[2], readBuffer[3]);
        printf("W25Q test complete\n\n");
        return;
    }

    // Test appending
    uint8_t testPacket2[16] = {0xAA, 0xBB, 0xBB, 0xAA, 0x01, 0x00, 0x10, 0x00, 0x10, 0x06, 'F', 'L', 'I', 'G', 'H', 'T'};

    printf("Writing second 16 byte test packet...\n");
    W25Q_WriteAppendData(testPacket2, sizeof(testPacket2));

    // Read back
    printf("Reading back data from address 0x%08lX...\n", DataStartAddr);
    uint8_t readBuffer2[16] = {0};
    W25Q_ReadVolumeSafe(DataStartAddr + sizeof(testPacket), readBuffer2, sizeof(readBuffer2));

    if (memcmp(testPacket2, readBuffer2, sizeof(testPacket2)) == 0) {
        printf("Readback matched the written packet exactly\n");
    } else {
        printf("Readback mismatch\n");
        printf("Expected sync word: %02X %02X %02X %02X\n", testPacket2[0], testPacket2[1], testPacket2[2], testPacket2[3]);
        printf("Actual sync word  : %02X %02X %02X %02X\n", readBuffer2[0], readBuffer2[1], readBuffer2[2], readBuffer2[3]);
        printf("W25Q test complete\n\n");
        return;
    }

    printf("W25Q test complete\n\n");
}



void Test_W25Q_BadBlocks() {
    printf("Starting W25Q bad block test\n");

    // Check initialization + JEDEC ID
    if (!InitialiseW25Q()) {
        printf("W25Q initialization failed\n");
        return;
    }
    printf("W25Q initialized - JEDEC ID verified\n");

    printf("Current section boundaries: MS=0x%08lX, ME=0x%08lX, DS=0x%08lX, DE=0x%08lX\n", MetaStartAddr, MetaEndAddr, DataStartAddr, DataEndAddr);

//    W25Q_ScanBadBlocks();
}









static void ProcessNMEASentence(const char *sentence) {
	switch (minmea_sentence_id(sentence, false)) {
		case MINMEA_SENTENCE_RMC: {
			struct minmea_sentence_rmc frame;
			if (minmea_parse_rmc(&frame, sentence)) {
				m10s_data.HasFix = frame.valid;

				if (frame.valid) {
					// Convert to standard floats
					m10s_data.Latitude = minmea_tocoord(&frame.latitude);
					m10s_data.Longitude = minmea_tocoord(&frame.longitude);
					m10s_data.Speed = minmea_tofloat(&frame.speed);

					m10s_data.Timestamp = (float)HAL_GetTick() / 1000.0f;
				}
			}
			break;
		}
		case MINMEA_SENTENCE_GGA: {
			struct minmea_sentence_gga frame;
			if (minmea_parse_gga(&frame, sentence)) {
				if (frame.fix_quality > 0) {
					m10s_data.Altitude = minmea_tofloat(&frame.altitude);
					m10s_data.Satellites = frame.satellites_tracked;
				}
			}
			break;
		}
		default:
			break;   // Ignore other sentences
	}
}


static void FeedI2CDataToParser(uint8_t *i2c_data, uint16_t length) {
	for (uint16_t i = 0; i < length; i++) {
		char c = (char)i2c_data[i];

		// Fill buffer
		if (line_len < M10S_LINE_BUFFER_SIZE - 1) {
			line_buffer[line_len++] = c;
		}

		// Wait for end of a sentence (\n)
		if (c == '\n') {
			line_buffer[line_len] = '\0';   // Add null terminator
			ProcessNMEASentence(line_buffer);
			line_len = 0;
		}
	}
}



void Poll_MAXM10S(void) {
	uint16_t bytes_available = MAXM10S_GetAvailableBytes();

	if (bytes_available > 0) {
		uint8_t i2c_data[bytes_available];
		ULED_TOGGLE

		if (MAXM10S_ReadStream(i2c_data, bytes_available)) {
			FeedI2CDataToParser(i2c_data, bytes_available);
		}
	}
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


	// MMC5983MA pins
	GPIO_InitStruct.Pin = MMC_INT_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(MMC_INT_PORT, &GPIO_InitStruct);

	HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);


	// W25Q pins
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);   // Default SPI CS pins to high
	GPIO_InitStruct.Pin = W25Q_CS_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(W25Q_CS_PORT, &GPIO_InitStruct);

	HAL_GPIO_WritePin(W25Q_WP_PORT, W25Q_WP_PIN, GPIO_PIN_SET);   // MUST BE HELD HIGH TO ALLOW WRITES TO STATUS REGISTER
	GPIO_InitStruct.Pin = W25Q_WP_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(W25Q_WP_PORT, &GPIO_InitStruct);

	HAL_GPIO_WritePin(W25Q_HOLD_PORT, W25Q_HOLD_PIN, GPIO_PIN_SET);   // MUST BE HELD HIGH TO ALLOW COMMUNICATIONS
	GPIO_InitStruct.Pin = W25Q_HOLD_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(W25Q_HOLD_PORT, &GPIO_InitStruct);
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
		mmc_data_ready = true;
		mmc_data_time = (float)uwTick / 1000.0f;
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
