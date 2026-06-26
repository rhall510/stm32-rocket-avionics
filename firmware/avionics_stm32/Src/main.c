#include "main.h"


void ReadIncomingLAMBDA80(void *param) {
	(void) param;

    while (1) {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
			if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
				printf("[ERROR] LAMBDA80 read timed out due to unreleased SPI mutex\n");
				return;
			}

			// Clear Rx interrupt
			LAMBDA80_ClearIRQ(&hspi3_rf, 0xFFFF, false);

			// Read the raw bytes from the radio buffer
			uint8_t len = 0;
			uint8_t start = 0;
			LAMBDA80_GetRxBufferStatus(&hspi3_rf, &len, &start, false);

			uint8_t buff[256];
			LAMBDA80_ReadBuffer(&hspi3_rf, buff, start, len, false);

			xSemaphoreGive(SPIRfMutex);

			// Decode raw bytes into a net packet
			NetPacket pkt;
			DecodeNetPacket(&pkt, buff, len);

			// Place into the radio response queue
			if (xQueueSend(RadioQueue, &pkt, pdMS_TO_TICKS(10)) != pdPASS) {
				printf("[ERROR] Radio response queue full\n");
			}
        }
    }
}


void ReadIncomingLAMBDA62(void *param) {
	(void) param;

    while (1) {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
			if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
				printf("[ERROR] LAMBDA62 read timed out due to unreleased SPI mutex\n");
				return;
			}

			// Clear Rx interrupt
			LAMBDA62_ClearIRQ(&hspi3_rf, 0xFFFF, false);

			// Read the raw bytes from the radio buffer
			uint8_t len = 0;
			uint8_t start = 0;
			LAMBDA62_GetRxBufferStatus(&hspi3_rf, &len, &start, false);

			uint8_t buff[256];
			LAMBDA62_ReadBuffer(&hspi3_rf, buff, start, len, false);

			xSemaphoreGive(SPIRfMutex);

			// Decode raw bytes into a net packet
			NetPacket pkt;
			DecodeNetPacket(&pkt, buff, len);

			// Place into the radio response queue
			if (xQueueSend(RadioQueue, &pkt, pdMS_TO_TICKS(10)) != pdPASS) {
				printf("[ERROR] Radio response queue full\n");
			}
        }
    }
}


void TransactionManagerTask(void *param) {
	(void) param;

	// State to function mapping table
	static const TMStateHandler TMStateTable[TM_NUM_STATES] = {
	    [TM_STATE_IDLE] = HandleStateIdle,
		[TM_DISC_CMD] = HandleStateDiscoveryCmd,
		[TM_PKTTEST_CMD] = HandleStatePktTestCmd
	};

    TMState currState = TM_STATE_IDLE;
    NetPacket currRadResp;

    // Continuously execute the function that maps to the current state and update the current state
    while (1) {
    	currState = TMStateTable[currState](&currRadResp);
    }
}


TMState HandleStateIdle(NetPacket* pkt) {
	// Wait for a command to arrive over radio
	if (xQueueReceive(RadioQueue, pkt, portMAX_DELAY) == pdPASS) {
		if (pkt->type == NET_MTYPE_DISCOVERY) { return TM_DISC_CMD; }
		if (pkt->type == NET_MTYPE_PKTTEST) { return TM_PKTTEST_CMD; }
	}
	return TM_STATE_IDLE;
}


TMState HandleStateDiscoveryCmd(NetPacket* pkt) {
//	printf("Sending ACK response to discovery call\n");

	// Stagger response by 50ms * address to prevent collisions
	vTaskDelay(pdMS_TO_TICKS(NET_ADDRESS * 50));

	if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(5)) != pdTRUE) {
		printf("[ERROR] Discovery ACK response timed out due to unreleased SPI mutex\n");
		return TM_STATE_IDLE;
	}

	NetPacket ackpkt;
	ackpkt.recipient = NET_CONTROLLER_ADDR;
	ackpkt.sender = NET_ADDRESS;
	ackpkt.status = 0x0;
	ackpkt.type = NET_MTYPE_ACK;
	ackpkt.seqnum = 0;
	ackpkt.payloadlen = 0;

	uint8_t buff[10];
	uint8_t len = ConstructNetPacket(buff, 10, &ackpkt);

	LAMBDA62_ClearIRQ(&hspi3_rf, 0xFFFF, false);
	LAMBDA62_SetPacketParamsFSK(&hspi3_rf, 32, 5, 64, 0, true, len, 2, false, false);

	xSemaphoreTake(LAMBDA62TxSemphr, 0);   // Clear any spurious Tx notifications
	LAMBDA62_SendPacket(&hspi3_rf, buff, len, false);
	xSemaphoreGive(SPIRfMutex);

	if (xSemaphoreTake(LAMBDA62TxSemphr, pdMS_TO_TICKS(200)) != pdTRUE) {
		printf("[ERROR] Discovery ACK response Tx timed out\n");
	}

	if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(20)) != pdTRUE) {
		printf("[ERROR] L62 not reset after discovery ACK due to unreleased SPI mutex\n");
		return TM_STATE_IDLE;
	}

	LAMBDA62_ClearIRQ(&hspi3_rf, 0xFFFF, false);   // Clear Tx interrupt
	LAMBDA62_SetPacketParamsFSK(&hspi3_rf, 32, 5, 64, 0, true, NET_PAYLOAD_MAXLEN, 2, false, false);
	LAMBDA62_SetRx(&hspi3_rf, 0xFFFFFF, false);

	xSemaphoreGive(SPIRfMutex);

	return TM_STATE_IDLE;
}


TMState HandleStatePktTestCmd(NetPacket* resp) {
	if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(20)) != pdTRUE) {
		printf("[ERROR] PKTTEST ACK timed out due to unreleased SPI mutex\n");
		return TM_STATE_IDLE;
	}

	NetPacket ackpkt;
	ackpkt.recipient = NET_CONTROLLER_ADDR;
	ackpkt.sender = NET_ADDRESS;
	ackpkt.status = 0x0;
	ackpkt.type = NET_MTYPE_ACK;
	ackpkt.seqnum = 0;

	// Echo the received sequence number on the same channel
	ackpkt.payloadlen = 5;
	ackpkt.payload[0] = resp->payload[0];   // Channel: 0 = 868MHz, 1 = 2.4GHz
	ackpkt.payload[1] = resp->payload[1];   // Sequence number [31:0]
	ackpkt.payload[2] = resp->payload[2];
	ackpkt.payload[3] = resp->payload[3];
	ackpkt.payload[4] = resp->payload[4];

	uint8_t buff[15];
	uint8_t len = ConstructNetPacket(buff, 15, &ackpkt);

	if (ackpkt.payload[0]) {   // Send the 2.4GHz response
//		printf("Sending PKTTEST ACK on 2.4GHz\n");

		LAMBDA80_ClearIRQ(&hspi3_rf, 0xFFFF, false);
		LAMBDA80_SetPacketParams(&hspi3_rf, 0x23, 0, len, 0x20, 0x40, false);

		xSemaphoreTake(LAMBDA80TxSemphr, 0);   // Clear any spurious Tx notifications
		LAMBDA80_SendPacket(&hspi3_rf, buff, len, false);
		xSemaphoreGive(SPIRfMutex);

		if (xSemaphoreTake(LAMBDA80TxSemphr, pdMS_TO_TICKS(150)) != pdTRUE) {
			printf("[ERROR] PKTTEST ACK L80 Tx timed out\n");
		}

		if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(20)) != pdTRUE) {
			printf("[ERROR] L80 not reset after PKTTEST ACK due to unreleased SPI mutex\n");
			return TM_STATE_IDLE;
		}

		LAMBDA80_ClearIRQ(&hspi3_rf, 0xFFFF, false);   // Clear Tx interrupt
		LAMBDA80_SetPacketParams(&hspi3_rf, 0x23, 0, NET_PAYLOAD_MAXLEN, 0x20, 0x40, false);
		LAMBDA80_SetRx(&hspi3_rf, 0, 0xFFFF, false);
	} else {   // Send the 868MHz response
//		printf("Sending PKTTEST ACK on 868MHz\n");

		LAMBDA62_ClearIRQ(&hspi3_rf, 0xFFFF, false);
		LAMBDA62_SetPacketParamsFSK(&hspi3_rf, 32, 5, 64, 0, true, len, 2, false, false);

		xSemaphoreTake(LAMBDA62TxSemphr, 0);   // Clear any spurious Tx notifications
		LAMBDA62_SendPacket(&hspi3_rf, buff, len, false);
		xSemaphoreGive(SPIRfMutex);

		if (xSemaphoreTake(LAMBDA62TxSemphr, pdMS_TO_TICKS(150)) != pdTRUE) {
			printf("[ERROR] PKTTEST ACK L62 Tx timed out\n");
		}

		if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(20)) != pdTRUE) {
			printf("[ERROR] L62 not reset after PKTTEST ACK due to unreleased SPI mutex\n");
			return TM_STATE_IDLE;
		}

		LAMBDA62_ClearIRQ(&hspi3_rf, 0xFFFF, false);   // Clear Tx interrupt
		LAMBDA62_SetPacketParamsFSK(&hspi3_rf, 32, 5, 64, 0, true, NET_PAYLOAD_MAXLEN, 2, false, false);
		LAMBDA62_SetRx(&hspi3_rf, 0xFFFFFF, false);
	}

	xSemaphoreGive(SPIRfMutex);

	return TM_STATE_IDLE;
}



int main(void) {
	// System init
	HAL_Init();

	SystemClockConfig();
	InitialiseGPIO();
	InitialiseI2C();
	InitialiseSPI();
	InitialiseCRC();
	InitialiseTimers();

//	HAL_Delay(2000);   // Startup delay to avoid code executing inbetween debug sessions

	__enable_irq();

	// Initialise RF modules
	InitialiseLAMBDA62FSK(&hspi3_rf, true);
	LAMBDA62_SetPacketParamsFSK(&hspi3_rf, 32, 5, 64, 0, true, NET_PAYLOAD_MAXLEN, 2, false, true);
	LAMBDA62_SetRx(&hspi3_rf, 0xFFFFFF, true);

	InitialiseLAMBDA80(&hspi3_rf, true);
	LAMBDA80_SetMode_Telemetry(&hspi3_rf, true);


	// Initialise FreeRTOS objects
	RadioQueue = xQueueCreate(5, sizeof(NetPacket));
	if (RadioQueue == NULL) { Error_Handler(); }

	SPIRfMutex = xSemaphoreCreateMutex();
	if (SPIRfMutex == NULL) { Error_Handler(); }

	LAMBDA80TxSemphr = xSemaphoreCreateBinary();
	if (LAMBDA80TxSemphr == NULL) { Error_Handler(); }

	LAMBDA62TxSemphr = xSemaphoreCreateBinary();
	if (LAMBDA62TxSemphr == NULL) { Error_Handler(); }


	// Create tasks
    xTaskCreate(ReadIncomingLAMBDA80, "LAMBDA80-Receive", 1024, NULL, 4, &LAMBDA80RxTaskNotif);
    xTaskCreate(ReadIncomingLAMBDA62, "LAMBDA62-Receive", 1024, NULL, 4, &LAMBDA62RxTaskNotif);
    xTaskCreate(TransactionManagerTask, "Transaction-Manager", 4096, NULL, 1, NULL);


    printf("INIT\n");

    vTaskStartScheduler();


	while (1){}
}




void TIM2_IRQHandler() {
	if (TIM2->SR & TIM_SR_UIF) {
		TIM2->SR &= ~TIM_SR_UIF;   // Clear interrupt flag
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
//    __HAL_RCC_SPI1_CLK_ENABLE();
//    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_SPI3_CLK_ENABLE();

//	// Initialise SPI1 (accelerometers)
//    GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
//    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//    GPIO_InitStruct.Pull = GPIO_PULLUP;
//    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
//    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
//    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
//
//	hspi1_acc.Instance = SPI1;
//	hspi1_acc.Init.Mode = SPI_MODE_MASTER;
//	hspi1_acc.Init.Direction = SPI_DIRECTION_2LINES;   // Standard full-duplex SPI
//	hspi1_acc.Init.DataSize = SPI_DATASIZE_8BIT;
//	hspi1_acc.Init.CLKPolarity = SPI_POLARITY_HIGH;   // Clock is high when idle
//	hspi1_acc.Init.CLKPhase = SPI_PHASE_2EDGE;   // Data is read on the rising edge of the clock
//	hspi1_acc.Init.NSS = SPI_NSS_SOFT;   // CS pins must be manually controlled
//	hspi1_acc.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;   // Must be low enough for ADXL 5MHz limit
//	hspi1_acc.Init.FirstBit = SPI_FIRSTBIT_MSB;
//	hspi1_acc.Init.TIMode = SPI_TIMODE_DISABLE;
//	hspi1_acc.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;   // Disable CRC check to save processing time
//	hspi1_acc.Init.CRCPolynomial = 7;
//	hspi1_acc.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
//	hspi1_acc.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;   // Enable CS pin pulse to reset logic between frames
//
//	if (HAL_SPI_Init(&hspi1_acc) != HAL_OK) { Error_Handler(); }
//
//
//	// Initialise SPI2 (storage)
//    GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
//    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//    GPIO_InitStruct.Pull = GPIO_NOPULL;
//    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
//    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
//    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
//
//	hspi2_str.Instance = SPI2;
//	hspi2_str.Init.Mode = SPI_MODE_MASTER;
//	hspi2_str.Init.Direction = SPI_DIRECTION_2LINES;
//	hspi2_str.Init.DataSize = SPI_DATASIZE_8BIT;
//	hspi2_str.Init.CLKPolarity = SPI_POLARITY_LOW;
//	hspi2_str.Init.CLKPhase = SPI_PHASE_1EDGE;
//	hspi2_str.Init.NSS = SPI_NSS_SOFT;
//	hspi2_str.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
//	hspi2_str.Init.FirstBit = SPI_FIRSTBIT_MSB;
//	hspi2_str.Init.TIMode = SPI_TIMODE_DISABLE;
//	hspi2_str.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
//	hspi2_str.Init.CRCPolynomial = 7;
//	hspi2_str.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
//	hspi2_str.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
//
//	if (HAL_SPI_Init(&hspi2_str) != HAL_OK) { Error_Handler(); }


	// Initialise SPI3 (RF communication)
//    GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
//    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//    GPIO_InitStruct.Pull = GPIO_NOPULL;
//    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
//    GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
//    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // 1. Initialise SCK (Pin 10) and MOSI (Pin 12) - No Pulls required
    GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // 2. Initialise MISO (Pin 11) - Pulldown applied to prevent floating
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    // Mode, Speed, and Alternate are already set correctly in the struct from above
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	hspi3_rf.Instance = SPI3;
	hspi3_rf.Init.Mode = SPI_MODE_MASTER;
	hspi3_rf.Init.Direction = SPI_DIRECTION_2LINES;
	hspi3_rf.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi3_rf.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi3_rf.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi3_rf.Init.NSS = SPI_NSS_SOFT;
	hspi3_rf.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
	hspi3_rf.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi3_rf.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi3_rf.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi3_rf.Init.CRCPolynomial = 7;
	hspi3_rf.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
	hspi3_rf.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;

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
	HAL_GPIO_WritePin(ULED1_PORT, ULED1_PIN, GPIO_PIN_RESET);
	GPIO_InitStruct.Pin = ULED1_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(ULED1_PORT, &GPIO_InitStruct);


	// UBUTTON
	GPIO_InitStruct.Pin = UBTN1_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(UBTN1_PORT, &GPIO_InitStruct);


//	// LSM6DSR pins
//	HAL_GPIO_WritePin(LSM6_CS_PORT, LSM6_CS_PIN, GPIO_PIN_SET);   // Default SPI CS pins to high
//	GPIO_InitStruct.Pin = LSM6_CS_PIN;
//	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
//	GPIO_InitStruct.Pull = GPIO_NOPULL;
//	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
//	HAL_GPIO_Init(LSM6_CS_PORT, &GPIO_InitStruct);
//
//
//	GPIO_InitStruct.Pin = LSM6_INT1_PIN;
//	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
//	GPIO_InitStruct.Pull = GPIO_NOPULL;
//	HAL_GPIO_Init(LSM6_INT1_PORT, &GPIO_InitStruct);
//
//
//	GPIO_InitStruct.Pin = LSM6_INT2_PIN;
//	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
//	GPIO_InitStruct.Pull = GPIO_NOPULL;
//	HAL_GPIO_Init(LSM6_INT2_PORT, &GPIO_InitStruct);
//
//	HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
//	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
//
//
//	// ADXL375 pins
//	HAL_GPIO_WritePin(ADXL_CS_PORT, ADXL_CS_PIN, GPIO_PIN_SET);   // Default SPI CS pins to high
//	GPIO_InitStruct.Pin = ADXL_CS_PIN;
//	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
//	GPIO_InitStruct.Pull = GPIO_NOPULL;
//	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
//	HAL_GPIO_Init(ADXL_CS_PORT, &GPIO_InitStruct);
//
//
//	GPIO_InitStruct.Pin = ADXL_INT1_PIN;
//	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
//	GPIO_InitStruct.Pull = GPIO_NOPULL;
//	HAL_GPIO_Init(ADXL_INT1_PORT, &GPIO_InitStruct);
//
//	HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
//	HAL_NVIC_EnableIRQ(EXTI0_IRQn);
//
//
//	GPIO_InitStruct.Pin = ADXL_INT2_PIN;
//	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
//	GPIO_InitStruct.Pull = GPIO_NOPULL;
//	HAL_GPIO_Init(ADXL_INT2_PORT, &GPIO_InitStruct);
//
//	HAL_NVIC_SetPriority(EXTI1_IRQn, 5, 0);
//	HAL_NVIC_EnableIRQ(EXTI1_IRQn);
//
//
//	// BMP581 pins
//	GPIO_InitStruct.Pin = BMP_INT_PIN;
//	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
//	GPIO_InitStruct.Pull = GPIO_NOPULL;
//	HAL_GPIO_Init(BMP_INT_PORT, &GPIO_InitStruct);
//
//	HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
//	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
//
//
//	// MMC5983MA pins
//	GPIO_InitStruct.Pin = MMC_INT_PIN;
//	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
//	GPIO_InitStruct.Pull = GPIO_NOPULL;
//	HAL_GPIO_Init(MMC_INT_PORT, &GPIO_InitStruct);
//
//	HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
//	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
//
//
//	// W25Q pins
//	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);   // Default SPI CS pins to high
//	GPIO_InitStruct.Pin = W25Q_CS_PIN;
//	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
//	GPIO_InitStruct.Pull = GPIO_NOPULL;
//	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
//	HAL_GPIO_Init(W25Q_CS_PORT, &GPIO_InitStruct);
//
//	HAL_GPIO_WritePin(W25Q_WP_PORT, W25Q_WP_PIN, GPIO_PIN_SET);   // MUST BE HELD HIGH TO ALLOW WRITES TO STATUS REGISTER
//	GPIO_InitStruct.Pin = W25Q_WP_PIN;
//	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
//	GPIO_InitStruct.Pull = GPIO_NOPULL;
//	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
//	HAL_GPIO_Init(W25Q_WP_PORT, &GPIO_InitStruct);
//
//	HAL_GPIO_WritePin(W25Q_HOLD_PORT, W25Q_HOLD_PIN, GPIO_PIN_SET);   // MUST BE HELD HIGH TO ALLOW COMMUNICATIONS
//	GPIO_InitStruct.Pin = W25Q_HOLD_PIN;
//	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
//	GPIO_InitStruct.Pull = GPIO_NOPULL;
//	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
//	HAL_GPIO_Init(W25Q_HOLD_PORT, &GPIO_InitStruct);


	// LAMBDA80 pins
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);   // Default SPI CS pins to high
	GPIO_InitStruct.Pin = L80_CS_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(L80_CS_PORT, &GPIO_InitStruct);

	HAL_GPIO_WritePin(L80_RST_PORT, L80_RST_PIN, GPIO_PIN_SET);   // Active low
	GPIO_InitStruct.Pin = L80_RST_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(L80_RST_PORT, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = L80_BUSY_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(L80_BUSY_PORT, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = L80_DIO1_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(L80_DIO1_PORT, &GPIO_InitStruct);

	HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

	GPIO_InitStruct.Pin = L80_DIO2_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(L80_DIO2_PORT, &GPIO_InitStruct);

	HAL_NVIC_SetPriority(EXTI3_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(EXTI3_IRQn);


	// LAMBDA62 pins
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);   // Default SPI CS pins to high
	GPIO_InitStruct.Pin = L62_CS_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(L62_CS_PORT, &GPIO_InitStruct);

	HAL_GPIO_WritePin(L62_RST_PORT, L62_RST_PIN, GPIO_PIN_SET);   // Active low
	GPIO_InitStruct.Pin = L62_RST_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(L62_RST_PORT, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = L62_BUSY_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(L62_BUSY_PORT, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = L62_DIO1_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(L62_DIO1_PORT, &GPIO_InitStruct);

	HAL_NVIC_SetPriority(EXTI4_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(EXTI4_IRQn);

	GPIO_InitStruct.Pin = L62_DIO2_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(L62_DIO2_PORT, &GPIO_InitStruct);

	HAL_NVIC_SetPriority(EXTI2_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(EXTI2_IRQn);

	HAL_GPIO_WritePin(L62_RXSW_PORT, L62_RXSW_PIN, GPIO_PIN_RESET);
	GPIO_InitStruct.Pin = L62_RXSW_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(L62_RXSW_PORT, &GPIO_InitStruct);

	HAL_GPIO_WritePin(L62_TXSW_PORT, L62_TXSW_PIN, GPIO_PIN_RESET);
	GPIO_InitStruct.Pin = L62_TXSW_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(L62_TXSW_PORT, &GPIO_InitStruct);
}


void InitialiseCRC() {
    RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN;   // Enable clock for the CRC peripheral
    __DSB();   // Ensure the clock is active before continuing

    CRC->CR |= CRC_CR_RESET;

    CRC->CR &= ~CRC_CR_POLYSIZE;   // Set polynomial size to 8 bit
    CRC->CR |= CRC_CR_POLYSIZE_1;
    CRC->POL = 0x07;   // Set polynomial value to 0x07.

    CRC->INIT = 0x00;   // Set initial value after reset

    CRC->CR &= ~(CRC_CR_REV_IN | CRC_CR_REV_OUT);   // Disable bit reversal
}


void InitialiseTimers() {
//	// TIM2
//	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;   // Enable clock
//    __DSB();   // Ensure the clock is active before continuing
//
//	TIM2->PSC = 16999;   // Set prescaler and ARR to generate 10Hz interrupts
//	TIM2->ARR = 10000 / FLASH_LOG_RATE - 1;
//
//	TIM2->DIER |= TIM_DIER_UIE;   // Enable update interrupt
//	NVIC_EnableIRQ(TIM2_IRQn);
//	NVIC_SetPriority(TIM2_IRQn, 5);
//
//	TIM2->CR1 |= TIM_CR1_CEN;   // Enable TIM2


	// TIM2 used as a microsecond clock
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;   // Enable clock
    __DSB();   // Ensure the clock is active before continuing

	TIM2->PSC = 143;   // Set prescaler to count once per microsecond (1MHz)
	TIM2->ARR |= 0xFFFFFFFF;   // Max auto reload for use as a microsecond timer

	TIM2->CR1 |= TIM_CR1_CEN;   // Enable TIM2
}



// External interrupt handlers
void EXTI0_IRQHandler(void) {
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_0);
}

void EXTI1_IRQHandler(void) {
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_1);
}

void EXTI2_IRQHandler(void) {   // LAMBDA62 Rx done
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_2);

	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		vTaskNotifyGiveFromISR(LAMBDA62RxTaskNotif, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

void EXTI3_IRQHandler(void) {   // LAMBDA80 Rx done
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_3);

	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		vTaskNotifyGiveFromISR(LAMBDA80RxTaskNotif, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

void EXTI4_IRQHandler(void) {   // LAMBDA62 Tx done
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_4);

	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xSemaphoreGiveFromISR(LAMBDA62TxSemphr, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

void EXTI9_5_IRQHandler(void) {
	if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_5) != RESET) {
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_5);
	} if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_6) != RESET) {   // LAMBDA80 Tx done
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_6);

		ULED1_TOGGLE

		if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
			BaseType_t xHigherPriorityTaskWoken = pdFALSE;
			xSemaphoreGiveFromISR(LAMBDA80TxSemphr, &xHigherPriorityTaskWoken);
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}
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


void DebugMon_Handler(void) {}

extern void xPortSysTickHandler(void);

void SysTick_Handler(void) {
	HAL_IncTick();

	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
		xPortSysTickHandler();
	}
}



// General error handler
void Error_Handler(void) {
    __disable_irq();
    while (1) {}
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    __disable_irq();
    while (1) {}
}
