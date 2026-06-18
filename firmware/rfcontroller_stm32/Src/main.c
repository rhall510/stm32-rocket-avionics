#include "main.h"


void RunTUDTask(void *param) {
	(void) param;

	while (1) {
		tud_task();
		vTaskDelay(pdMS_TO_TICKS(1));
	}
}


void ReadIncomingUSB(void *param) {
    (void) param;

    while (1) {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
        	printf("Reading USB command\n");
        	while (tud_cdc_available() > 0) {
				uint8_t buff[512];
				uint32_t count = tud_cdc_read(buff, sizeof(buff));
				ParseUSBBytes(buff, count);
			}
        }
    }
}


void ReadIncomingLAMBDA80(void *param) {
	(void) param;

    while (1) {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
			if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
				printf("Reading L80\n");
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
				if (xQueueSend(RadioResponseQueue, &pkt, pdMS_TO_TICKS(10)) != pdPASS) {
					printf("[ERROR] Radio response queue full\n");
				}
			} else {
				printf("[ERROR] LAMBDA80 read timed out due to unreleased SPI mutex\n");
			}
        }
    }
}


void ReadIncomingLAMBDA62(void *param) {
	(void) param;

    while (1) {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
        	if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        		printf("Reading L62\n");

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
    			if (xQueueSend(RadioResponseQueue, &pkt, pdMS_TO_TICKS(10)) != pdPASS) {
    				printf("[ERROR] Radio response queue full\n");
    			}
        	} else {
        		printf("[ERROR] LAMBDA62 read timed out due to unreleased SPI mutex\n");
        	}
        }
    }
}


void TransactionManagerTask(void *param) {
	(void) param;

	// State to function mapping table
	static const TMStateHandler TMStateTable[TM_NUM_STATES] = {
	    [TM_STATE_IDLE] = HandleStateIdle,
	    [TM_ECHO_CMD] = HandleStateEchoCmd,
		[TM_STATUS_CMD] = HandleStateStatusCmd,
		[TM_DISC_CMD] = HandleStateDiscoveryCmd
	};

    TMState currState = TM_STATE_IDLE;
    USBPacket currCmd;
    NetPacket currRadResp;

    // Continuously execute the function that maps to the current state and update the current state
    while (1) {
    	currState = TMStateTable[currState](&currCmd, &currRadResp);
    }
}


TMState HandleStateIdle(USBPacket* pkt, NetPacket* resp) {
	// Wait for a command to arrive over USB
	if (xQueueReceive(CommandQueue, pkt, portMAX_DELAY) == pdPASS) {
		if (pkt->type == USB_MTYPE_ECHO) { return TM_ECHO_CMD; }
		if (pkt->type == USB_MTYPE_STATUS) { return TM_STATUS_CMD; }
		if (pkt->type == USB_MTYPE_DISCOVERY) { return TM_DISC_CMD; }
	}
	return TM_STATE_IDLE;
}


TMState HandleStateEchoCmd(USBPacket* pkt, NetPacket* resp) {
	printf("Executing ECHO command\n");
	SendPacketUSB(pkt);
	return TM_STATE_IDLE;
}


TMState HandleStateStatusCmd(USBPacket* pkt, NetPacket* resp) {
	printf("Executing STATUS command\n");

	pkt->payloadlen = 2;
	pkt->payload[0] = 0xFA;
	pkt->payload[1] = 0xBB;

	SendPacketUSB(pkt);
	return TM_STATE_IDLE;
}


TMState HandleStateDiscoveryCmd(USBPacket* pkt, NetPacket* resp) {
	static TickType_t startTime = 0;
	static bool isFirstCall = true;

	// Initialise radio, send the discovery packet, and start the 500ms timer on first pass
	if (isFirstCall) {
		if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
			printf("Executing DISCOVERY command\n");

			USBPacket status;
			status.type = USB_MTYPE_STATUS;
			status.payloadlen = 1;
			status.payload[0] = 0xDD;

			SendPacketUSB(&status);

			// Broadcast the discovery packet
			NetPacket discpkt;
			discpkt.recipient = NET_BROADCAST_ADDR;
			discpkt.sender = NET_CONTROLLER_ADDR;
			discpkt.status = 0x0;
			discpkt.type = NET_MTYPE_DISCOVERY;
			discpkt.seqnum = 0;
			discpkt.payloadlen = 0;

			uint8_t buff[10];
			uint8_t len = ConstructNetPacket(buff, 10, &discpkt);

			LAMBDA62_ClearIRQ(&hspi3_rf, 0xFFFF, false);
			LAMBDA62_SetPacketParamsFSK(&hspi3_rf, 32, 7, 32, 0, true, len, 2, false, false);
			LAMBDA62_SendPacket(&hspi3_rf, buff, len, false);

			// Wait for Tx to finish before continuing
			if (xSemaphoreTake(LAMBDA62TxSemphr, pdMS_TO_TICKS(200)) == pdTRUE) {
				// Clear Tx interrupt
				LAMBDA62_ClearIRQ(&hspi3_rf, 0xFFFF, false);

				// Set to Rx mode for 500ms
				LAMBDA62_SetRx(&hspi3_rf, 0xFFFFFF, false);

				xSemaphoreGive(SPIRfMutex);

				startTime = xTaskGetTickCount();
				isFirstCall = false;
			} else {
				xSemaphoreGive(SPIRfMutex);
				printf("[ERROR] Discovery packet Tx timed out\n");
				return TM_STATE_IDLE;
			}
		} else {
			return TM_STATE_IDLE;
		}
	}

	// Try to receive ACKs from the radio response queue
	if (xQueueReceive(RadioResponseQueue, resp, pdMS_TO_TICKS(10)) == pdPASS) {
		if (resp->type == NET_MTYPE_ACK) {
			USBPacket status;
			status.type = USB_MTYPE_STATUS;
			status.payloadlen = 2;
			status.payload[0] = 0xDD;
			status.payload[1] = resp->sender;

			SendPacketUSB(&status);
		}
	}

	if ((xTaskGetTickCount() - startTime) >= pdMS_TO_TICKS(500)) {
		// ACK receive window closed
		isFirstCall = true;
		return TM_STATE_IDLE;
	}

	// ACK receive window still open, stay in this state
	return TM_DISC_CMD;
}


int main(void) {
    HAL_Init();

	SystemClockConfig();
	InitialiseGPIO();
	InitialiseSPI();
	InitialiseTimers();

	InitialiseLAMBDA62FSK(&hspi3_rf, true);

	__enable_irq();

	tusb_init();


	// Initialise FreeRTOS objects
	RadioResponseQueue = xQueueCreate(5, sizeof(NetPacket));
	if (RadioResponseQueue == NULL) { Error_Handler(); }

	CommandQueue = xQueueCreate(5, sizeof(USBPacket));
	if (CommandQueue == NULL) { Error_Handler(); }

	USBTxMutex = xSemaphoreCreateMutex();
	if (USBTxMutex == NULL) { Error_Handler(); }

	SPIRfMutex = xSemaphoreCreateMutex();
	if (SPIRfMutex == NULL) { Error_Handler(); }

	LAMBDA80TxSemphr = xSemaphoreCreateBinary();
	if (LAMBDA80TxSemphr == NULL) { Error_Handler(); }

	LAMBDA62TxSemphr = xSemaphoreCreateBinary();
	if (LAMBDA62TxSemphr == NULL) { Error_Handler(); }


	// Create tasks
    xTaskCreate(RunTUDTask, "TUSB-Task", 1024, NULL, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(ReadIncomingUSB, "CMD-Receive", 1024, NULL, 4, &USBRxTaskNotif);
    xTaskCreate(ReadIncomingLAMBDA80, "LAMBDA80-Receive", 1024, NULL, 4, &LAMBDA80RxTaskNotif);
    xTaskCreate(ReadIncomingLAMBDA62, "LAMBDA62-Receive", 1024, NULL, 4, &LAMBDA62RxTaskNotif);
    xTaskCreate(TransactionManagerTask, "Transaction-Manager", 4096, NULL, 1, NULL);

    DiscoveryTimer = xTimerCreate("DiscTimer", pdMS_TO_TICKS(10000), pdTRUE, (void *)0, SendDiscoveryPacket);
    xTimerStart(DiscoveryTimer, 0);


    printf("INIT\n");

    vTaskStartScheduler();

    while (1) {}
}



void SendPacketUSB(USBPacket* packetinfo) {
    if (!tud_cdc_connected()) { return; }

    // Construct a raw byte stream from the packet info
    uint8_t buff[256];
    uint16_t len = ConstructUSBPacket(buff, 256, packetinfo);

    // Gated on mutex to prevent collisions if multiple tasks write to USB
    if (xSemaphoreTake(USBTxMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        uint16_t bytes_sent = 0;

        while (bytes_sent < len) {
            if (!tud_cdc_connected()) { break; }

            uint32_t available = tud_cdc_write_available();

            if (available > 0) {
            	// Break longer transfers into chunks that fit the buffer
                uint16_t chunk_size = (len - bytes_sent < available) ? (len - bytes_sent) : available;
                uint32_t written = tud_cdc_write(&buff[bytes_sent], chunk_size);
                bytes_sent += written;

                tud_cdc_write_flush();
            }

            if (bytes_sent < len) {
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }

        // Release USB Tx mutex when finished
        xSemaphoreGive(USBTxMutex);
    } else {
    	printf("[ERROR] USB write timed out\n");
    }
}



void ParseUSBBytes(uint8_t *bytestream, uint16_t len) {
    static USBParserState state = USBPARSER_WAIT_SYNC_HIGH;
    static uint16_t payloadidx = 0;
    static USBPacket packet;

    for (uint16_t i = 0; i < len; i++) {
    	uint8_t byte = bytestream[i];

        switch (state) {
            case USBPARSER_WAIT_SYNC_HIGH:
                if (byte == (USB_SYNC_WORD >> 8)) {
                    state = USBPARSER_WAIT_SYNC_LOW;
                }
                break;

            case USBPARSER_WAIT_SYNC_LOW:
                if (byte == (USB_SYNC_WORD & 0xFF)) {
                    state = USBPARSER_TYPE;
                } else if (byte != (USB_SYNC_WORD >> 8)) {
					state = USBPARSER_WAIT_SYNC_HIGH;
				}
                break;

            case USBPARSER_TYPE:
            	packet.type = byte;
            	state = USBPARSER_LEN;
            	break;

            case USBPARSER_LEN:
                packet.payloadlen = byte;
                payloadidx = 0;

            	if (packet.payloadlen == 0) {
					if (xQueueSend(CommandQueue, &packet, pdMS_TO_TICKS(10)) != pdPASS) {
						printf("[ERROR] USB command queue full\n");
					}
					state = USBPARSER_WAIT_SYNC_HIGH;
				} else {
					state = USBPARSER_PAYLOAD;
				}

                break;

            case USBPARSER_PAYLOAD:
                packet.payload[payloadidx++] = byte;

            	if (payloadidx >= packet.payloadlen) {
					if (xQueueSend(CommandQueue, &packet, pdMS_TO_TICKS(10)) != pdPASS) {
						printf("[ERROR] USB command queue full\n");
					}
					state = USBPARSER_WAIT_SYNC_HIGH;
				} else if (payloadidx >= USB_PAYLOAD_MAXLEN) {
                	printf("[ERROR] USB parser found longer than allowed payload\n");
                    state = USBPARSER_WAIT_SYNC_HIGH;
                }

                break;
        }
    }
}



void SendDiscoveryPacket(TimerHandle_t Timer) {
	// Adds a discovery command to the command queue when triggered by DiscoveryTimer
	USBPacket disccmd;
	disccmd.type = USB_MTYPE_DISCOVERY;
	disccmd.payloadlen = 0;

	if (xQueueSend(CommandQueue, &disccmd, pdMS_TO_TICKS(10)) != pdPASS) {
		printf("[ERROR] USB command queue full\n");
	}
}






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
    RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
    RCC_OscInitStruct.PLL.PLLN = 18;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV6;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

    __HAL_RCC_PLLCLKOUT_ENABLE(RCC_PLL_48M1CLK);

    // Initialise CPU, AHB and APB bus clocks
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) { Error_Handler(); }

    // Turn on USB peripheral clock with HSE
	RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
	PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL;

	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) { Error_Handler(); }
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

	HAL_GPIO_WritePin(ULED2_PORT, ULED2_PIN, GPIO_PIN_RESET);
	GPIO_InitStruct.Pin = ULED2_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(ULED2_PORT, &GPIO_InitStruct);


	// UBUTTON
	GPIO_InitStruct.Pin = UBTN1_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(UBTN1_PORT, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = UBTN2_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(UBTN2_PORT, &GPIO_InitStruct);


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


	// Enable USB clock
	__HAL_RCC_USB_CLK_ENABLE();
	HAL_PWREx_DisableUCPDDeadBattery();

	// Set USB interrupt priorities (initialisation handled by tusb)
	HAL_NVIC_SetPriority(USB_LP_IRQn, 5, 0);
	HAL_NVIC_SetPriority(USB_HP_IRQn, 5, 0);
	HAL_NVIC_SetPriority(USBWakeUp_IRQn, 5, 0);
}


void InitialiseSPI() {
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	// Initialise SPI3
	__HAL_RCC_SPI3_CLK_ENABLE();

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
	hspi3_rf.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
	hspi3_rf.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi3_rf.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi3_rf.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi3_rf.Init.CRCPolynomial = 7;
	hspi3_rf.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
	hspi3_rf.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;

	if (HAL_SPI_Init(&hspi3_rf) != HAL_OK) { Error_Handler(); }
}


void InitialiseTimers() {
	// TIM2 used as a microsecond clock
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;   // Enable clock
    __DSB();   // Ensure the clock is active before continuing

	TIM2->PSC = 143;   // Set prescaler to count once per microsecond (1MHz)
	TIM2->ARR |= 0xFFFFFFFF;   // Max auto reload for use as a microsecond timer

	TIM2->CR1 |= TIM_CR1_CEN;   // Enable TIM2
}


// TUSB callbacks
void tud_cdc_rx_cb(uint8_t itf) {
	xTaskNotifyGive(USBRxTaskNotif);
}


// USB interrupts
void USB_LP_IRQHandler(void) {
    tud_int_handler(0);
}

void USB_HP_IRQHandler(void) {
    tud_int_handler(0);
}

void USBWakeUP_IRQHandler(void) {
    tud_int_handler(0);
}


// External interrupt handlers
void EXTI2_IRQHandler(void) {   // LAMBDA62 Rx done
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_2);

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	vTaskNotifyGiveFromISR(LAMBDA62RxTaskNotif, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void EXTI3_IRQHandler(void) {   // LAMBDA80 Rx done
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_3);

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	vTaskNotifyGiveFromISR(LAMBDA80RxTaskNotif, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void EXTI4_IRQHandler(void) {   // LAMBDA62 Tx done
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_4);

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xSemaphoreGiveFromISR(LAMBDA62TxSemphr, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void EXTI9_5_IRQHandler(void) {
	if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_6) != RESET) {   // LAMBDA80 Tx done
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_6);

		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xSemaphoreGiveFromISR(LAMBDA80TxSemphr, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
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



