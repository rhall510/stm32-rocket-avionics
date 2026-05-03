#include "main.h"
#include "lambda80.h"


volatile bool PacketReady = false;

SPI_HandleTypeDef hspi3_rf;

UART_HandleTypeDef huart2;


void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);



int main(void) {
	HAL_Init();

	SystemClock_Config();

	MX_GPIO_Init();
	MX_SPI1_Init();
	MX_USART2_UART_Init();

	InitialiseLAMBDA80();
	LAMBDA80_SetMode_Download();
//	LAMBDA80_SetPacketParams(0x23, 0, 11, 0x20, 0x40);
	LAMBDA80_SetRx(0, 0xFFFF);

	BSP_LED_Init(LED_GREEN);


	uint8_t send[] = "Hello from STM32!\r\n";
	HAL_UART_Transmit(&huart2, send, sizeof(send) - 1, HAL_MAX_DELAY);

	while (1) {
		if (PacketReady) {
			PacketReady = false;

			uint8_t len = 0;
			uint8_t start = 0;

			LAMBDA80_GetRxBufferStatus(&len, &start);

			if (len > 0) {
				uint8_t send[len];
				LAMBDA80_ReadBuffer(send, start, len);

				HAL_UART_Transmit(&huart2, send, len, HAL_MAX_DELAY);

				uint8_t newline[] = "\r\n";
				HAL_UART_Transmit(&huart2, newline, 2, HAL_MAX_DELAY);
			}

			LAMBDA80_ClearIRQ(0xFFFF);
		}
    }
}


void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
	RCC_OscInitStruct.PLL.PLLN = 85;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
	RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}


	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
							  |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
		Error_Handler();
	}
}


static void MX_SPI1_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_SPI1_CLK_ENABLE();

	GPIO_InitStruct.Pin = GPIO_PIN_7 | GPIO_PIN_6 | GPIO_PIN_5;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	hspi3_rf.Instance = SPI1;
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
	if (HAL_SPI_Init(&hspi3_rf) != HAL_OK) {
		Error_Handler();
	}
}


static void MX_USART2_UART_Init(void) {
	__HAL_RCC_USART2_CLK_ENABLE();

	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	huart2.Instance = USART2;
	huart2.Init.BaudRate = 1000000;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
	huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

	if (HAL_UART_Init(&huart2) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK) {
		Error_Handler();
	}
}


static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

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

	HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

	GPIO_InitStruct.Pin = L80_DIO2_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(L80_DIO2_PORT, &GPIO_InitStruct);

	HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}


void EXTI0_IRQHandler(void) {
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_0);
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
		PacketReady = true;
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


void Error_Handler(void) {
  __disable_irq();
  while (1) {}
}

