#include "main.h"



// Task function for TinyUSB
void usb_device_task(void *param) {
    (void) param;
    tusb_init();

    while (1) {
        tud_task(); // TinyUSB device task
        // We do not add a vTaskDelay here; tud_task needs to run continuously
    }
}

// Task function for your custom logic (e.g., handling RF telemetry)
void telemetry_task(void *param) {
    (void) param;

    while (1) {
        // Your logic here...

        // Example: Check if USB CDC is connected and send data
        if (tud_cdc_connected()) {
            const char *msg = "Telemetry Packet\r\n";
            tud_cdc_write(msg, strlen(msg));
            tud_cdc_write_flush();
        }

        // Yield to other tasks for 10ms
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

int main(void) {
    HAL_Init();
    // SystemClock_Config(); (Ensure HSI48 is set up for USB here)

    // Create the USB task
    xTaskCreate(usb_device_task, "USB", configMINIMAL_STACK_SIZE * 2, NULL, configMAX_PRIORITIES - 1, NULL);

    // Create your application task
    xTaskCreate(telemetry_task, "Telemetry", configMINIMAL_STACK_SIZE * 2, NULL, 1, NULL);

    // Hand over control to FreeRTOS
    vTaskStartScheduler();

    // The code should never reach here unless there was insufficient RAM to start the OS
    while (1) {}
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
