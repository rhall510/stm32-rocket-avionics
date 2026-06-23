#include "lambda62.h"


uint8_t LAMBDA62_Status(SPI_HandleTypeDef *hspi, bool Blocking) {
	LAMBDA62_WaitBusy(Blocking);

    uint8_t tx[2] = {L62_STATUS, 0};
    uint8_t rx[2] = {0};

    HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(hspi, tx, rx, 2, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

    return rx[1];
}


uint16_t LAMBDA62_DevErrors(SPI_HandleTypeDef *hspi, bool Blocking) {
	LAMBDA62_WaitBusy(Blocking);

    uint8_t tx[4] = {L62_DEV_ERRORS, 0, 0, 0};
    uint8_t rx[4] = {0};

    HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(hspi, tx, rx, 4, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

    return (((uint16_t)rx[2] << 8) | rx[3]) & 0b1111111010000000;   // Mask only non reserved bits
}


inline bool LAMBDA62_CheckBusy() {
	return HAL_GPIO_ReadPin(L62_BUSY_PORT, L62_BUSY_PIN);
}


void LAMBDA62_WaitBusy(bool Blocking) {
	if (Blocking) {
		while (LAMBDA62_CheckBusy()) {}
		return;
	}

    uint32_t startus = GET_MICROS;
    bool firstpass = true;

    while(LAMBDA62_CheckBusy()) {
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


void LAMBDA62_ClearIRQ(SPI_HandleTypeDef *hspi, uint16_t IRQMask, bool Blocking) {
	LAMBDA62_WaitBusy(Blocking);

	uint8_t tx[3] = {L62_CLEAR_IRQ, (uint8_t)(IRQMask >> 8), (uint8_t)IRQMask};

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 3, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);
}


uint16_t LAMBDA62_GetIRQStatus(SPI_HandleTypeDef *hspi, bool Blocking) {
	LAMBDA62_WaitBusy(Blocking);

    uint8_t tx[4] = {L62_IRQ_STATUS, 0, 0, 0};
    uint8_t rx[4] = {0};

    HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(hspi, tx, rx, 4, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

    return ((uint16_t)rx[2] << 8) | rx[3];
}


void InitialiseLAMBDA62LoRa(SPI_HandleTypeDef *hspi, bool Blocking) {
	uint8_t tx[5] = {0};

	// Hardware reset
	HAL_GPIO_WritePin(L62_RST_PORT, L62_RST_PIN, GPIO_PIN_RESET);
	HAL_Delay(1);
	HAL_GPIO_WritePin(L62_RST_PORT, L62_RST_PIN, GPIO_PIN_SET);

	LAMBDA62_WaitBusy(Blocking);   // Wait for startup


	// Set packet type to LoRa
	tx[0] = L62_PKT_TYPE;
	tx[1] = 0x1;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	LAMBDA62_WaitBusy(Blocking);

	// Set frequency to 868MHz
	tx[0] = L62_RF_FREQ;
	tx[1] = 0x36;
	tx[2] = 0x40;
	tx[3] = 0x00;
	tx[4] = 0x00;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 5, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	LAMBDA62_WaitBusy(Blocking);

	// Set PA config (DutyCycle, hpMax, deviceSel, paLut)
	tx[0] = L62_PA_CFG;
	tx[1] = 0x04;
	tx[2] = 0x07;
	tx[3] = 0x00;
	tx[4] = 0x01;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 5, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	LAMBDA62_WaitBusy(Blocking);

	// Set TX params (Power, RampTime)
	tx[0] = L62_TX_PARAMS;
	tx[1] = 0x16;
	tx[2] = 0x04;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 3, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	LAMBDA62_WaitBusy(Blocking);

	// Optimise TxClampConfig to minimise losses in case of antenna mismatch (as per datasheet section 15.2)
	uint8_t txconfig = LAMBDA62_ReadReg(hspi, L62_TXCLAMP, Blocking);
	txconfig |= 0x1E;
	LAMBDA62_WriteReg(hspi, L62_TXCLAMP, txconfig, Blocking);

	LAMBDA62_WaitBusy(Blocking);

	// Set buffer base addresses (TX, RX)
	tx[0] = L62_BUFF_BASE_ADDR;
	tx[1] = L62_TX_BASE_ADDR;
	tx[2] = L62_RX_BASE_ADDR;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 3, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	LAMBDA62_WaitBusy(Blocking);

	// Set modulation params (SF, BW, CR, LDR_OP)
	tx[0] = L62_MOD_PARAMS;
	tx[1] = 0x07;
	tx[2] = 0x05;
	tx[3] = 0x01;
	tx[4] = 0x00;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 5, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	// Set default packet parameters
	LAMBDA62_SetPacketParamsLoRa(hspi, 8, 0, 10, 1, 0, Blocking);   // 8 bit preamble, explicit header, CRC on, normal IQ

	LAMBDA62_WaitBusy(Blocking);

	// Set Tx done interrupt on DIO1 and Rx done interrupt on DIO2
	uint8_t irq[9] = {L62_SET_IRQ, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00};

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, irq, 9, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	LAMBDA62_WaitBusy(Blocking);

	LAMBDA62_ClearIRQ(hspi, 0xFFFF, Blocking);
}


void InitialiseLAMBDA62FSK(SPI_HandleTypeDef *hspi, bool Blocking) {
	uint8_t tx[9] = {0};

	// Hardware reset
	HAL_GPIO_WritePin(L62_RST_PORT, L62_RST_PIN, GPIO_PIN_RESET);
	HAL_Delay(1);
	HAL_GPIO_WritePin(L62_RST_PORT, L62_RST_PIN, GPIO_PIN_SET);

	HAL_Delay(5);

	LAMBDA62_WaitBusy(Blocking);   // Wait for startup

	// Set packet type to FSK
	tx[0] = L62_PKT_TYPE;
	tx[1] = 0x0;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	LAMBDA62_WaitBusy(Blocking);

	// Image calibration for 863-870MHz
	tx[0] = L62_IMG_CAL;
	tx[1] = 0xD7;
	tx[2] = 0xDB;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 3, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	LAMBDA62_WaitBusy(Blocking);


	// Set frequency to 868MHz
	tx[0] = L62_RF_FREQ;
	tx[1] = 0x36;
	tx[2] = 0x40;
	tx[3] = 0x00;
	tx[4] = 0x00;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 5, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	LAMBDA62_WaitBusy(Blocking);

	// Set PA config (DutyCycle, hpMax, deviceSel, paLut)
	tx[0] = L62_PA_CFG;
	tx[1] = 0x04;
	tx[2] = 0x07;
	tx[3] = 0x00;
	tx[4] = 0x01;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 5, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	LAMBDA62_WaitBusy(Blocking);

	// Set TX params (Power, RampTime)
	tx[0] = L62_TX_PARAMS;
	tx[1] = 0x16;
	tx[2] = 0x04;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 3, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	LAMBDA62_WaitBusy(Blocking);

	// Optimise TxClampConfig to minimise losses in case of antenna mismatch (as per datasheet section 15.2)
	uint8_t txconfig = LAMBDA62_ReadReg(hspi, L62_TXCLAMP, Blocking);
	txconfig |= 0x1E;
	LAMBDA62_WriteReg(hspi, L62_TXCLAMP, txconfig, Blocking);

	LAMBDA62_WaitBusy(Blocking);

	// Set to boosted gain mode - from datasheet
	LAMBDA62_WriteReg(hspi, 0x08AC, 0x96, Blocking);
	LAMBDA62_WaitBusy(Blocking);

	// Set sync word
	LAMBDA62_WriteReg(hspi, 0x06C0, 0xC3, Blocking);
	LAMBDA62_WriteReg(hspi, 0x06C1, 0x22, Blocking);
	LAMBDA62_WriteReg(hspi, 0x06C2, 0x33, Blocking);
	LAMBDA62_WriteReg(hspi, 0x06C3, 0x44, Blocking);
	LAMBDA62_WriteReg(hspi, 0x06C4, 0x55, Blocking);
	LAMBDA62_WriteReg(hspi, 0x06C5, 0x66, Blocking);
	LAMBDA62_WriteReg(hspi, 0x06C6, 0x77, Blocking);
	LAMBDA62_WriteReg(hspi, 0x06C7, 0x88, Blocking);

	// Set buffer base addresses (TX, RX)
	tx[0] = L62_BUFF_BASE_ADDR;
	tx[1] = L62_TX_BASE_ADDR;
	tx[2] = L62_RX_BASE_ADDR;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 3, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	LAMBDA62_WaitBusy(Blocking);

	// Set modulation params (BR[3], SHAPE, BW, FDEV[3])
	tx[0] = L62_MOD_PARAMS;
	tx[1] = 0x00;
	tx[2] = 0x10;
	tx[3] = 0x00;
	tx[4] = 0x09;
	tx[5] = 0x09;
	tx[6] = 0x01;
	tx[7] = 0x00;
	tx[8] = 0x00;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 9, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	// Set default packet parameters
	LAMBDA62_SetPacketParamsFSK(hspi, 32, 5, 32, 0, true, 10, 2, false, Blocking);

	LAMBDA62_WaitBusy(Blocking);

	// Set Tx done interrupt on DIO1 and Rx done interrupt on DIO2
	uint8_t irq[9] = {L62_SET_IRQ, 0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00};

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, irq, 9, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	LAMBDA62_WaitBusy(Blocking);

	LAMBDA62_ClearIRQ(hspi, 0xFFFF, Blocking);

	LAMBDA62_WaitBusy(Blocking);
}


void LAMBDA62_SetTx(SPI_HandleTypeDef *hspi, uint32_t Timeout, bool Blocking) {
	LAMBDA62_WaitBusy(Blocking);

	HAL_GPIO_WritePin(L62_RXSW_PORT, L62_RXSW_PIN, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(L62_TXSW_PORT, L62_TXSW_PIN, GPIO_PIN_SET);

	uint8_t tx[4] = {L62_SETTX, (uint8_t)(Timeout >> 16), (uint8_t)(Timeout >> 8), (uint8_t)Timeout};

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	LAMBDA62_WaitBusy(Blocking);
}


void LAMBDA62_SendPacket(SPI_HandleTypeDef *hspi, uint8_t *packet, uint8_t len, bool Blocking) {
	LAMBDA62_WaitBusy(Blocking);

	// Send opcode with offset then packet buffer
	uint8_t tx[2] = {L62_WRITE_BUFF, L62_TX_BASE_ADDR};

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 2, HAL_MAX_DELAY);
	HAL_SPI_Transmit(hspi, packet, len, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	LAMBDA62_SetTx(hspi, L62_TX_TIMEOUT, Blocking);
}


void LAMBDA62_SetPacketParamsLoRa(SPI_HandleTypeDef *hspi, uint16_t PreambleLen, uint8_t HeaderType, uint8_t len,
								  uint8_t CRCType, uint8_t InvertIQ, bool Blocking) {
	LAMBDA62_WaitBusy(Blocking);

	uint8_t tx[7] = {L62_PKT_PARAMS, (uint8_t)(PreambleLen >> 8), (uint8_t)PreambleLen, HeaderType, len, CRCType, InvertIQ};

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 7, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	LAMBDA62_WaitBusy(Blocking);
}


void LAMBDA62_SetPacketParamsFSK(SPI_HandleTypeDef *hspi, uint16_t PreambleLen, uint8_t PreambleDetectLen, uint8_t SyncWordLen,
								 uint8_t AddrComp, bool ExplicitLength, uint8_t len, uint8_t CRCType, bool Whitening, bool Blocking) {
	LAMBDA62_WaitBusy(Blocking);

	uint8_t tx[10];
	tx[0] = L62_PKT_PARAMS;
	tx[1] = (uint8_t)(PreambleLen >> 8);
	tx[2] = (uint8_t)PreambleLen;
	tx[3] = PreambleDetectLen;
	tx[4] = SyncWordLen;
	tx[5] = AddrComp;
	tx[6] = ExplicitLength ? 0x1 : 0x0;
	tx[7] = len;
	tx[8] = CRCType;
	tx[9] = Whitening ? 0x1 : 0x0;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 10, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	LAMBDA62_WaitBusy(Blocking);
}


void LAMBDA62_SetRx(SPI_HandleTypeDef *hspi, uint32_t Timeout, bool Blocking) {
	LAMBDA62_WaitBusy(Blocking);

	HAL_GPIO_WritePin(L62_TXSW_PORT, L62_TXSW_PIN, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(L62_RXSW_PORT, L62_RXSW_PIN, GPIO_PIN_SET);

	uint8_t tx[4] = {L62_SETRX, (uint8_t)(Timeout >> 16), (uint8_t)(Timeout >> 8), (uint8_t)Timeout};

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);
}


void LAMBDA62_GetRxBufferStatus(SPI_HandleTypeDef *hspi, uint8_t *len, uint8_t *start, bool Blocking) {
	LAMBDA62_WaitBusy(Blocking);

	uint8_t tx[4] = {L62_RX_BUFF_STATUS, 0, 0, 0};   	// Skip status return on 2nd byte
	uint8_t rx[4] = {0};

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(hspi, tx, rx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	*len = rx[2];
	*start = rx[3];
}


void LAMBDA62_ReadBuffer(SPI_HandleTypeDef *hspi, uint8_t *buff, uint8_t StartAddr, uint8_t len, bool Blocking) {
	LAMBDA62_WaitBusy(Blocking);

	// Send opcode with offset then read into buffer
	uint8_t tx[3] = {L62_READ_BUFF, StartAddr, 0};

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 3, HAL_MAX_DELAY);
	HAL_SPI_Receive(hspi, buff, len, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);
}


void LAMBDA62_GetPktStatusLoRa(SPI_HandleTypeDef *hspi, int8_t *rssi, int8_t *snr, int8_t *sigrssi, bool Blocking) {
	// This function returns the ACTUAL values for RSSI and SNR
	// Actual RSSI = -Rssi[...] / 2
	// Actual SNR = SnrPkt / 4

	LAMBDA62_WaitBusy(Blocking);

    uint8_t tx[5] = {L62_PKT_STATUS, 0, 0, 0, 0};
    uint8_t rx[5] = {0};

    HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(hspi, tx, rx, 5, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

    *rssi = -rx[2] / 2;
    *snr = rx[3] / 4;
    *sigrssi = -rx[4] / 2;
}


void LAMBDA62_GetPktStatusFSK(SPI_HandleTypeDef *hspi, uint8_t *rxstatus, int8_t *rssisync, int8_t *rssiavg, bool Blocking) {
	// This function returns the ACTUAL values for RSSI
	// Actual RSSI = -Rssi[...] / 2

	LAMBDA62_WaitBusy(Blocking);

    uint8_t tx[5] = {L62_PKT_STATUS, 0, 0, 0, 0};
    uint8_t rx[5] = {0};

    HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(hspi, tx, rx, 5, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

    *rxstatus = rx[2];
    *rssisync = -rx[3] / 2;
    *rssiavg = -rx[4] / 2;
}



uint8_t LAMBDA62_ReadReg(SPI_HandleTypeDef *hspi, uint16_t RegAddr, bool Blocking) {
	LAMBDA62_WaitBusy(Blocking);

	uint8_t tx[5] = {0};   	// Skip status return on 4th byte
	uint8_t rx[5] = {0};

	tx[0] = L62_READ_REG;
	tx[1] = (uint8_t)(RegAddr >> 8);
	tx[2] = (uint8_t)RegAddr;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(hspi, tx, rx, 5, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	return rx[4];
}


void LAMBDA62_WriteReg(SPI_HandleTypeDef *hspi, uint16_t RegAddr, uint8_t val, bool Blocking) {
	LAMBDA62_WaitBusy(Blocking);

	uint8_t tx[4];

	tx[0] = L62_WRITE_REG;
	tx[1] = (uint8_t)(RegAddr >> 8);
	tx[2] = (uint8_t)RegAddr;
	tx[3] = val;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, tx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	LAMBDA62_WaitBusy(Blocking);
}


void LAMBDA62_SendContinuousWave(SPI_HandleTypeDef *hspi, bool Blocking) {
	LAMBDA62_WaitBusy(Blocking);

	uint8_t tx = L62_SETCW;

	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, &tx, 1, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(L62_CS_PORT, L62_CS_PIN, GPIO_PIN_SET);

	LAMBDA62_WaitBusy(Blocking);
}


