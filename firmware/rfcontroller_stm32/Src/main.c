#include "main.h"


void RunTUDTask(void *param) {
	(void) param;

	while (1) {
		tud_task();
		vTaskDelay(pdMS_TO_TICKS(1));
	}
}


void CheckIncomingCMD(void *param) {
    (void) param;

    while (1) {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
        	while (tud_cdc_available() > 0) {
				uint8_t buff[64];
				uint32_t count = tud_cdc_read(buff, sizeof(buff));

				for (int i = 0; i < count; i++) {
					printf("%c", (char)buff[i]);
				}

		        SendPacketUSB(buff, count);
			}
        }
    }
}



int main(void) {
    HAL_Init();

	SystemClockConfig();
	InitialiseGPIO();
	InitialiseSPI();

	__enable_irq();

	tusb_init();

	printf("INIT\n");

    xTaskCreate(RunTUDTask, "TUSB-Task", 1024, NULL, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(CheckIncomingCMD, "CMD-Receive", 512, NULL, 1, &xUSBRx);

    vTaskStartScheduler();

    while (1) {}
}



void SendPacketUSB(uint8_t *buff, uint16_t len) {
    if (tud_cdc_connected()) {
        tud_cdc_write(buff, len);
        tud_cdc_write_flush();
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



// TUSB callbacks
void tud_cdc_rx_cb(uint8_t itf) {
	xTaskNotifyGive(xUSBRx);
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
void EXTI2_IRQHandler(void) {
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_2);
}

void EXTI3_IRQHandler(void) {
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_3);
}

void EXTI4_IRQHandler(void) {
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_4);
}

void EXTI9_5_IRQHandler(void) {
	if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_6) != RESET) {
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_6);
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



