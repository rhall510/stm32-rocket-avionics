#include "lambda80.h"


uint8_t LAMBDA80_Status(SPI_HandleTypeDef *hspi, bool Blocking) {
	LAMBDA80_WaitBusy(Blocking);

    uint8_t tx[2] = {L80_STATUS, 0};
    uint8_t rx[2] = {0};

    HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(hspi, tx, rx, 2, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

    return rx[1];
}


inline bool LAMBDA80_CheckBusy() {
	return HAL_GPIO_ReadPin(L80_BUSY_PORT, L80_BUSY_PIN);
}


void LAMBDA80_WaitBusy(bool Blocking) {
	if (Blocking) {
		while (LAMBDA80_CheckBusy()) {}
		return;
	}

    uint32_t startus = GET_MICROS;
    bool firstpass = true;

    while(LAMBDA80_CheckBusy()) {
        if (firstpass) {   // Initially poll for 50us to catch quick events
            uint32_t currus = GET_MICROS - startus;

            if (currus > 50) {
            	firstpass = false;
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        } else {   // Once over 1ms has passed, yield immediately once checked
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}


void InitialiseLAMBDA80(SPI_HandleTypeDef *hspi, bool Blocking) {
	uint8_t tx[5] = {0};

	// Hardware reset
	HAL_GPIO_WritePin(L80_RST_PORT, L80_RST_PIN, GPIO_PIN_RESET);
	HAL_Delay(1);
	HAL_GPIO_WritePin(L80_RST_PORT, L80_RST_PIN, GPIO_PIN_SET);

	LAMBDA80_WaitBusy(Blocking);   // Wait for startup

	// Set packet type to LoRa
	tx[0] = L80_PKT_TYPE;
	tx[1] = 0x1;

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

	LAMBDA80_WaitBusy(Blocking);

	// Set frequency to 2.4GHz (values from datasheet)
	tx[0] = L80_RF_FREQ;
	tx[1] = 0xB8;
	tx[2] = 0x9D;
	tx[3] = 0x89;

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

	LAMBDA80_WaitBusy(Blocking);

	// Set TX params (Power, RampTime)
	tx[0] = L80_TX_PARAMS;
	tx[1] = 0x1F;
	tx[2] = 0x80;

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 3, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

	LAMBDA80_WaitBusy(Blocking);

	// Set buffer base addresses (TX, RX)
	tx[0] = L80_BUFF_BASE_ADDR;
	tx[1] = L80_TX_BASE_ADDR;
	tx[2] = L80_RX_BASE_ADDR;

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 3, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

	LAMBDA80_WaitBusy(Blocking);

	// Set Tx done interrupt on DIO1 and Rx done interrupt on DIO2
	uint8_t irq[9] = {L80_SET_IRQ, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00};

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, irq, 9, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

	LAMBDA80_WaitBusy(Blocking);
}


void LAMBDA80_SetMode_Telemetry(SPI_HandleTypeDef *hspi, bool Blocking){
	LAMBDA80_WaitBusy(Blocking);

	uint8_t tx[5] = {0};

	// Switch to standby mode
	tx[0] = L80_STDBY;
	tx[1] = 0x00;
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

	LAMBDA80_WaitBusy(Blocking);

	// Set modulation params (SF, BW, CR)
	tx[0] = L80_MOD_PARAMS;
	tx[1] = 0xA0;   // SF10
	tx[2] = 0x18;   // BW 812.5KHz
	tx[3] = 0x01;   // CR 4/5

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

	LAMBDA80_WaitBusy(Blocking);

	LAMBDA80_WriteReg(hspi, 0x925, 0x32, Blocking);   // From datasheet for SF10

	// Set default packet parameters
	LAMBDA80_SetPacketParams(hspi, 0x23, 0, 30, 0x20, 0x40, Blocking);   // 12 bit preamble, explicit header, 30 byte payload, CRC on, normal IQ

	LAMBDA80_SetRx(hspi, 2, 0, Blocking);   // Set to Rx continuous mode
}


void LAMBDA80_SetMode_Download(SPI_HandleTypeDef *hspi, bool Blocking) {
	LAMBDA80_WaitBusy(Blocking);

	uint8_t tx[5] = {0};

	// Switch to standby mode
	tx[0] = L80_STDBY;
	tx[1] = 0x00;
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

	LAMBDA80_WaitBusy(Blocking);

	// Set modulation params (SF, BW, CR)
	tx[0] = L80_MOD_PARAMS;
	tx[1] = 0x50;   // SF5
	tx[2] = 0x0A;   // BW 1625KHz
	tx[3] = 0x01;   // CR 4/5

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

	LAMBDA80_WaitBusy(Blocking);

	LAMBDA80_WriteReg(hspi, 0x925, 0x1E, Blocking);   // From datasheet for SF5

	// Set default packet parameters
	LAMBDA80_SetPacketParams(hspi, 0x23, 0, 112, 0x20, 0x40, Blocking);   // 12 bit preamble, explicit header, 112 byte payload, CRC on, normal IQ

	LAMBDA80_WaitBusy(Blocking);
}


void LAMBDA80_ClearIRQ(SPI_HandleTypeDef *hspi, uint16_t IRQMask, bool Blocking) {
	LAMBDA80_WaitBusy(Blocking);

	uint8_t tx[3] = {L80_CLEAR_IRQ, (uint8_t)(IRQMask >> 8), (uint8_t)IRQMask};

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 3, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);
}


uint16_t LAMBDA80_GetIRQStatus(SPI_HandleTypeDef *hspi, bool Blocking) {
	LAMBDA80_WaitBusy(Blocking);

    uint8_t tx[4] = {L80_IRQ_STATUS, 0, 0, 0};
    uint8_t rx[4] = {0};

    HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(hspi, tx, rx, 4, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

    return ((uint16_t)rx[2] << 8) | rx[3];
}



void LAMBDA80_SetTx(SPI_HandleTypeDef *hspi, uint8_t TimeBase, uint16_t Timeout, bool Blocking) {
	LAMBDA80_WaitBusy(Blocking);

	// Timeout is timebase (see below for mappings) * timeout, 0 = no timeout
	// 0x00 15.625 μs
	// 0x01 62.5 μs
	// 0x02 1 ms
	// 0x03 4 ms
	uint8_t tx[4] = {L80_SETTX, TimeBase, (uint8_t)(Timeout >> 8), (uint8_t)Timeout};

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);
}


void LAMBDA80_SendPacket(SPI_HandleTypeDef *hspi, uint8_t *packet, uint8_t len, bool Blocking) {
	LAMBDA80_WaitBusy(Blocking);

	// Send opcode with offset then packet buffer
	uint8_t tx[2] = {L80_WRITE_BUFF, L80_TX_BASE_ADDR};

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 2, HAL_MAX_DELAY);
	HAL_SPI_Transmit(hspi, packet, len, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

	LAMBDA80_WaitBusy(Blocking);

	LAMBDA80_SetTx(hspi, 0x2, L80_TX_TIMEOUT, Blocking);
}


void LAMBDA80_SetPacketParams(SPI_HandleTypeDef *hspi, uint8_t PreambleLen, uint8_t HeaderType, uint8_t len,
							  uint8_t CRCType, uint8_t InvertIQ, bool Blocking) {
	LAMBDA80_WaitBusy(Blocking);

	uint8_t tx[8] = {L80_PKT_PARAMS, PreambleLen, HeaderType, len, CRCType, InvertIQ, 0x0, 0x0};   // Last 2 bytes not used for LoRa

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 8, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);
}


void LAMBDA80_SetRx(SPI_HandleTypeDef *hspi, uint8_t TimeBase, uint16_t Timeout, bool Blocking) {
	LAMBDA80_WaitBusy(Blocking);

	// Timeout is timebase (see below for mappings) * timeout, 0 = no timeout, 0xFFFF = continuous mode
	// 0x00 15.625 μs
	// 0x01 62.5 μs
	// 0x02 1 ms
	// 0x03 4 ms
	uint8_t tx[4] = {L80_SETRX, TimeBase, (uint8_t)(Timeout >> 8), (uint8_t)Timeout};

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);
}


void LAMBDA80_GetRxBufferStatus(SPI_HandleTypeDef *hspi, uint8_t *len, uint8_t *start, bool Blocking) {
	LAMBDA80_WaitBusy(Blocking);

	uint8_t tx[4] = {L80_RX_BUFF_STATUS, 0, 0, 0};   	// Skip status return on 2nd byte
	uint8_t rx[4] = {0};

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(hspi, tx, rx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

	*len = rx[2];
	*start = rx[3];
}


void LAMBDA80_ReadBuffer(SPI_HandleTypeDef *hspi, uint8_t *buff, uint8_t StartAddr, uint8_t len, bool Blocking) {
	LAMBDA80_WaitBusy(Blocking);

	// Send opcode with offset then read into buffer
	uint8_t tx[3] = {L80_READ_BUFF, StartAddr, 0};

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 3, HAL_MAX_DELAY);
	HAL_SPI_Receive(hspi, buff, len, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);
}


void LAMBDA80_GetPktStatusLoRa(SPI_HandleTypeDef *hspi, int8_t *rssi, int8_t *snr, bool Blocking) {
	// This function returns the ACTUAL values for RSSI and SNR
	// Actual RSSI = -rssiSync / 2
	// Actual SNR = snr / 4

	LAMBDA80_WaitBusy(Blocking);

    uint8_t tx[4] = {L80_PKT_STATUS, 0, 0, 0};
    uint8_t rx[4] = {0};

    HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(hspi, tx, rx, 5, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

    *rssi = -rx[2] / 2;
    *snr = rx[3] / 4;
}

void LAMBDA80_GetPktStatusFSK(SPI_HandleTypeDef *hspi, int8_t *rssiSync, uint8_t *errors, uint8_t *status, uint8_t *sync, bool Blocking) {
	// This function returns the ACTUAL value for RSSI
	// Actual RSSI = -rssiSync / 2

	LAMBDA80_WaitBusy(Blocking);

    uint8_t tx[7] = {L80_PKT_STATUS, 0, 0, 0, 0, 0, 0};
    uint8_t rx[7] = {0};

    HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(hspi, tx, rx, 7, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

    *rssiSync = -rx[3] / 2;
    *errors = rx[4];
    *status = rx[5];
    *sync = rx[6];
}



uint8_t LAMBDA80_ReadReg(SPI_HandleTypeDef *hspi, uint16_t RegAddr, bool Blocking) {
	LAMBDA80_WaitBusy(Blocking);

	uint8_t tx[5] = {0};   	// Skip status return on 4th byte
	uint8_t rx[5] = {0};

	tx[0] = L80_READ_REG;
	tx[1] = (uint8_t)(RegAddr >> 8);
	tx[2] = (uint8_t)RegAddr;

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(hspi, tx, rx, 5, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);

	return rx[4];
}


void LAMBDA80_WriteReg(SPI_HandleTypeDef *hspi, uint16_t RegAddr, uint8_t val, bool Blocking) {
	LAMBDA80_WaitBusy(Blocking);

	uint8_t tx[4];

	tx[0] = L80_WRITE_REG;
	tx[1] = (uint8_t)(RegAddr >> 8);
	tx[2] = (uint8_t)RegAddr;
	tx[3] = val;

	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L80_CS_PORT, L80_CS_PIN, GPIO_PIN_SET);
}




