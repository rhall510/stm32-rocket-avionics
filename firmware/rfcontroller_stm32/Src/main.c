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
//        	printf("Reading USB command\n");
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
			if (xQueueSend(RadioResponseQueue, &pkt, pdMS_TO_TICKS(10)) != pdPASS) {
				printf("[ERROR] Radio response queue full\n");
			}
        }
    }
}


void ReadIncomingLAMBDA62(void *param) {
	(void) param;

    while (1) {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
        	printf("L62\n");
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
			if (xQueueSend(RadioResponseQueue, &pkt, pdMS_TO_TICKS(10)) != pdPASS) {
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
	    [TM_ECHO_CMD] = HandleStateEchoCmd,
		[TM_STATUS_CMD] = HandleStateStatusCmd,
		[TM_DISC_CMD] = HandleStateDiscoveryCmd,
		[TM_PKTTEST_CMD] = HandleStatePktTestCmd,
		[TM_DATA_DOWNLOAD_CMD] = HandleStateDataDownloadCmd
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
	if (xQueueReceive(CommandQueue, pkt, pdMS_TO_TICKS(50)) == pdPASS) {
		if (pkt->type == USB_MTYPE_ECHO) { return TM_ECHO_CMD; }
		if (pkt->type == USB_MTYPE_STATUS) { return TM_STATUS_CMD; }
		if (pkt->type == USB_MTYPE_DISCOVERY) { return TM_DISC_CMD; }
		if (pkt->type == USB_MTYPE_PKTTEST) { return TM_PKTTEST_CMD; }
		if (pkt->type == USB_MTYPE_DATA_DOWNLOAD) { return TM_DATA_DOWNLOAD_CMD; }
	}
	return TM_STATE_IDLE;
}


TMState HandleStateEchoCmd(USBPacket* pkt, NetPacket* resp) {
//	printf("Executing ECHO command\n");

	SendPacketUSB(pkt);
	return TM_STATE_IDLE;
}


TMState HandleStateStatusCmd(USBPacket* pkt, NetPacket* resp) {
//	printf("Executing STATUS command\n");

	pkt->payloadlen = 2;
	pkt->payload[0] = 0xFA;
	pkt->payload[1] = 0xBB;

	SendPacketUSB(pkt);
	return TM_STATE_IDLE;
}


TMState HandleStateDiscoveryCmd(USBPacket* pkt, NetPacket* resp) {
	// Initialise radio, send the discovery packet, and start the 500ms timer on first pass
	if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
		printf("[ERROR] DISCOVERY timed out due to unreleased SPI mutex\n");
		return TM_STATE_IDLE;
	}

//	printf("Executing DISCOVERY command\n");

	// Send USB packet notifying discovery packet sent
	USBPacket status;
	status.type = USB_MTYPE_INFO;
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
	LAMBDA62_SetPacketParamsFSK(&hspi3_rf, 32, 5, 64, 0, true, len, 2, false, false);

	xSemaphoreTake(LAMBDA62TxSemphr, 0);   // Clear any spurious Tx notifications
	LAMBDA62_SendPacket(&hspi3_rf, buff, len, false);
	xSemaphoreGive(SPIRfMutex);

	// Wait for Tx to finish before continuing
	if (xSemaphoreTake(LAMBDA62TxSemphr, pdMS_TO_TICKS(150)) != pdTRUE) {
		printf("[ERROR] Discovery packet Tx timed out\n");
		return TM_STATE_IDLE;
	}

	if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
		printf("[ERROR] L62 not reset after discovery Tx due to unreleased SPI mutex\n");
		return TM_STATE_IDLE;
	}

	// Clear Tx interrupt
	LAMBDA62_ClearIRQ(&hspi3_rf, 0xFFFF, false);

	// Set to Rx continuous mode
	LAMBDA62_SetPacketParamsFSK(&hspi3_rf, 32, 5, 64, 0, true, NET_PAYLOAD_MAXLEN, 2, false, false);
	LAMBDA62_SetRx(&hspi3_rf, 0xFFFFFF, false);

	xSemaphoreGive(SPIRfMutex);


	// Try to receive ACKs from the radio response queue for 500ms
	TickType_t startTime = xTaskGetTickCount();
	while ((xTaskGetTickCount() - startTime) < pdMS_TO_TICKS(500)) {
		if (xQueueReceive(RadioResponseQueue, resp, pdMS_TO_TICKS(10)) == pdPASS) {
			if (resp->type == NET_MTYPE_ACK) {
				// Send USB notification of response
				USBPacket status;
				status.type = USB_MTYPE_DISCOVERY;
				status.payloadlen = 1;
				status.payload[0] = resp->sender;

				SendPacketUSB(&status);
			}
		}
	}

	return TM_STATE_IDLE;
}


TMState HandleStatePktTestCmd(USBPacket* pkt, NetPacket* resp) {
	if (!(pkt->type == USB_MTYPE_PKTTEST && pkt->payloadlen == 1)) {
		printf("Invalid USB packet format for packet test command");
		return TM_STATE_IDLE;
	}

	// Pause discovery packets while PKTTEST is running
	xTimerStop(DiscoveryTimer, 0);

	uint8_t TargetAddr = pkt->payload[0];
	uint32_t SeqNum = 0;

	// Whether to send the packet request on 2.4GHz or 868MHz
	bool RequestOn24 = false;

	// Continuously request test packets from the targeted node
	while (1) {
		// Check for stop commands at the start of each loop
		if (xQueueReceive(CommandQueue, pkt, pdMS_TO_TICKS(1)) == pdPASS) {
			if (pkt->type == USB_MTYPE_STOP) {
				xTimerReset(DiscoveryTimer, 0);
				return TM_STATE_IDLE;
			}
		}

		if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
			printf("[ERROR] Packet test stopped due to unreleased RF SPI mutex\n");
			xTimerReset(DiscoveryTimer, 0);
			return TM_STATE_IDLE;
		}

		SeqNum++;

		// Send the request packet
		NetPacket discpkt;
		discpkt.recipient = TargetAddr;
		discpkt.sender = NET_CONTROLLER_ADDR;
		discpkt.status = 0x0;
		discpkt.type = NET_MTYPE_PKTTEST;
		discpkt.seqnum = 0;
		discpkt.payloadlen = 5;

		discpkt.payload[0] = RequestOn24;
		discpkt.payload[1] = (int8_t)(SeqNum >> 24);
		discpkt.payload[2] = (int8_t)(SeqNum >> 16);
		discpkt.payload[3] = (int8_t)(SeqNum >> 8);
		discpkt.payload[4] = (int8_t)SeqNum;


		uint8_t buff[15];
		uint8_t len = ConstructNetPacket(buff, 15, &discpkt);

		if (RequestOn24) {
			LAMBDA80_ClearIRQ(&hspi3_rf, 0xFFFF, false);
			LAMBDA80_SetPacketParams(&hspi3_rf, 0x23, 0, len, 0x20, 0x40, false);

			xSemaphoreTake(LAMBDA80TxSemphr, 0);   // Clear any spurious Tx notifications
			LAMBDA80_SendPacket(&hspi3_rf, buff, len, false);
			xSemaphoreGive(SPIRfMutex);

			// Wait for Tx to finish before continuing
			if (xSemaphoreTake(LAMBDA80TxSemphr, pdMS_TO_TICKS(150)) != pdTRUE) {
				printf("[ERROR] Packet test stopped due to L80 Tx time out\n");
				xTimerReset(DiscoveryTimer, 0);
				return TM_STATE_IDLE;
			}

			if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
				printf("[ERROR] Packet test stopped due to unreleased RF SPI mutex. L80 could not be reset\n");
				xTimerReset(DiscoveryTimer, 0);
				return TM_STATE_IDLE;
			}

			// Clear Tx interrupt
			LAMBDA80_ClearIRQ(&hspi3_rf, 0xFFFF, false);

			// Set to Rx continuous mode
			LAMBDA80_SetPacketParams(&hspi3_rf, 0x23, 0, NET_PAYLOAD_MAXLEN, 0x20, 0x40, false);
			LAMBDA80_SetRx(&hspi3_rf, 0, 0xFFFF, false);

			xSemaphoreGive(SPIRfMutex);
		} else {
			LAMBDA62_ClearIRQ(&hspi3_rf, 0xFFFF, false);
			LAMBDA62_SetPacketParamsFSK(&hspi3_rf, 32, 5, 64, 0, true, len, 2, false, false);

			xSemaphoreTake(LAMBDA62TxSemphr, 0);   // Clear any spurious Tx notifications
			LAMBDA62_SendPacket(&hspi3_rf, buff, len, false);
			xSemaphoreGive(SPIRfMutex);


			// Wait for Tx to finish before continuing
			if (xSemaphoreTake(LAMBDA62TxSemphr, pdMS_TO_TICKS(150)) != pdTRUE) {
				printf("[ERROR] Packet test stopped due to L62 Tx time out\n");
				xTimerReset(DiscoveryTimer, 0);
				return TM_STATE_IDLE;
			}

			if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
				printf("[ERROR] Packet test stopped due to unreleased RF SPI mutex. L62 could not be reset\n");
				xTimerReset(DiscoveryTimer, 0);
				return TM_STATE_IDLE;
			}

			// Clear Tx interrupt
			LAMBDA62_ClearIRQ(&hspi3_rf, 0xFFFF, false);

			// Set to Rx mode for 500ms
			LAMBDA62_SetPacketParamsFSK(&hspi3_rf, 32, 5, 64, 0, true, NET_PAYLOAD_MAXLEN, 2, false, false);
			LAMBDA62_SetRx(&hspi3_rf, 0xFFFFFF, false);

			xSemaphoreGive(SPIRfMutex);
		}


		// Try to receive ACKs from the radio response queue for 500ms
		TickType_t recStartTime = xTaskGetTickCount();
		bool received = false;
		while ((xTaskGetTickCount() - recStartTime) < pdMS_TO_TICKS(500)) {
			if (xQueueReceive(RadioResponseQueue, resp, pdMS_TO_TICKS(5)) != pdPASS) { continue; }
			if (resp->type != NET_MTYPE_ACK || resp->sender != TargetAddr || resp->payloadlen != 5) { continue; }

			received = true;

			if (resp->payload[0]) {
				int8_t snr = 0;
				int8_t rssi = 0;

				if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
					LAMBDA80_GetPktStatusLoRa(&hspi3_rf, &rssi, &snr, false);
					xSemaphoreGive(SPIRfMutex);
				}

				USBPacket status;
				status.type = USB_MTYPE_PKTTEST;
				status.payloadlen = 8;
				status.payload[0] = 0x01;   // Channel
				status.payload[1] = TargetAddr;
				status.payload[2] = resp->payload[1];
				status.payload[3] = resp->payload[2];
				status.payload[4] = resp->payload[3];
				status.payload[5] = resp->payload[4];
				status.payload[6] = rssi;
				status.payload[7] = snr;

				SendPacketUSB(&status);
			} else {
				uint8_t rxstatus = 0;
				int8_t rssisync = 0;
				int8_t rssiavg = 0;

				if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
					LAMBDA62_GetPktStatusFSK(&hspi3_rf, &rxstatus, &rssisync, &rssiavg, false);
					xSemaphoreGive(SPIRfMutex);
				}

				USBPacket status;
				status.type = USB_MTYPE_PKTTEST;
				status.payloadlen = 7;
				status.payload[0] = 0x00;   // Channel
				status.payload[1] = TargetAddr;
				status.payload[2] = resp->payload[1];
				status.payload[3] = resp->payload[2];
				status.payload[4] = resp->payload[3];
				status.payload[5] = resp->payload[4];
				status.payload[6] = rssiavg;

				SendPacketUSB(&status);
			}
		}

		if (!received) {
			USBPacket status;
			status.type = USB_MTYPE_PKTTEST;
			status.payloadlen = 1;
			status.payload[0] = RequestOn24 | 0x80;   // Channel with MSB set indicates no response

			SendPacketUSB(&status);
		}

		RequestOn24 = !RequestOn24;
	}
}


TMState HandleStateDataDownloadCmd(USBPacket* pkt, NetPacket* resp) {
	// Pause discovery packets while download is running
	xTimerStop(DiscoveryTimer, 0);

	// Extract requested data range
	uint32_t NumReqBytes = ((uint32_t)pkt->payload[0] << 24) | ((uint32_t)pkt->payload[1] << 16) |
						   ((uint32_t)pkt->payload[2] << 8) | pkt->payload[3];
	uint32_t ReqByteOffset = ((uint32_t)pkt->payload[4] << 24) | ((uint32_t)pkt->payload[5] << 16) |
						     ((uint32_t)pkt->payload[6] << 8) | pkt->payload[7];

	// Get the number of available data bytes
	NetPacket dwnpkt;
	dwnpkt.recipient = NET_AVIONICS_ADDR;
	dwnpkt.sender = NET_CONTROLLER_ADDR;
	dwnpkt.status = 0x0;
	dwnpkt.type = NET_MTYPE_GET_DATA_RANGE;
	dwnpkt.seqnum = 0;
	dwnpkt.payloadlen = 0;

	uint8_t buff[NET_PACKET_MAXLEN];   // Max length to use for receiving data later
	uint8_t len = ConstructNetPacket(buff, NET_PACKET_MAXLEN, &dwnpkt);


	if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(20)) != pdTRUE) {
		printf("[ERROR] Data download timed out due to unreleased SPI mutex\n");
		xTimerReset(DiscoveryTimer, 0);   // Restart discovery calls
		return TM_STATE_IDLE;
	}

	LAMBDA62_ClearIRQ(&hspi3_rf, 0xFFFF, false);
	LAMBDA62_SetPacketParamsFSK(&hspi3_rf, 32, 5, 64, 0, true, len, 2, false, false);

	xSemaphoreTake(LAMBDA62TxSemphr, 0);   // Clear any spurious Tx notifications
	LAMBDA62_SendPacket(&hspi3_rf, buff, len, false);
	xSemaphoreGive(SPIRfMutex);

	// Wait for Tx to finish before continuing
	if (xSemaphoreTake(LAMBDA62TxSemphr, pdMS_TO_TICKS(100)) != pdTRUE) {
		printf("[ERROR] Data request Tx timed out\n");
		xTimerReset(DiscoveryTimer, 0);   // Restart discovery calls
		return TM_STATE_IDLE;
	}

	if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(20)) != pdTRUE) {
		printf("[ERROR] L62 not reset after discovery Tx due to unreleased SPI mutex\n");
		xTimerReset(DiscoveryTimer, 0);   // Restart discovery calls
		return TM_STATE_IDLE;
	}

	// Clear Tx interrupt
	LAMBDA62_ClearIRQ(&hspi3_rf, 0xFFFF, false);

	// Set to Rx continuous mode
	LAMBDA62_SetPacketParamsFSK(&hspi3_rf, 32, 5, 64, 0, true, NET_PAYLOAD_MAXLEN, 2, false, false);
	LAMBDA62_SetRx(&hspi3_rf, 0xFFFFFF, false);

	xSemaphoreGive(SPIRfMutex);


	// Wait for response
	if (xQueueReceive(RadioResponseQueue, resp, pdMS_TO_TICKS(100)) != pdPASS) {
		printf("[ERROR] Data boundary request Rx timed out\n");
		xTimerReset(DiscoveryTimer, 0);   // Restart discovery calls
		return TM_STATE_IDLE;
	}

	if (resp->type != NET_MTYPE_GET_DATA_RANGE) {
		printf("[ERROR] Bad data request Rx\n");
		xTimerReset(DiscoveryTimer, 0);   // Restart discovery calls
		return TM_STATE_IDLE;
	}

	// Extract available bytes and relay to host
	uint32_t NumAvailBytes = ((uint32_t)resp->payload[0] << 24) | ((uint32_t)resp->payload[1] << 16) |
							 ((uint32_t)resp->payload[2] << 8) | resp->payload[3];

	if (NumReqBytes == 0) {   // Request all available if NumReqBytes is 0
		NumReqBytes = NumAvailBytes;
	}

	USBPacket relaypkt;
	relaypkt.type = USB_MTYPE_INFO;
	relaypkt.payloadlen = 8;

	for (int i = 0; i < 8; i++) {   // Copy over received data values
		relaypkt.payload[i] = resp->payload[i];
	}

	SendPacketUSB(&relaypkt);


	// Initialise USB packet for relaying data
	relaypkt.type = USB_MTYPE_DATA_DOWNLOAD;

	// Initialise SX1280 for high bandwidth transmission
	if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(20)) != pdTRUE) {
		printf("[ERROR] Data download timed out due to unreleased SPI mutex\n");
		xTimerReset(DiscoveryTimer, 0);   // Restart discovery calls
		return TM_STATE_IDLE;
	}

	LAMBDA80_SetMode_Download(&hspi3_rf, false);
	LAMBDA80_SetPacketParams(&hspi3_rf, 0x23, 0, NET_PACKET_MAXLEN, 0x20, 0x40, false);
	LAMBDA80_ClearIRQ(&hspi3_rf, 0xFFFF, false);
	LAMBDA80_SetRx(&hspi3_rf, 2, 0xFFFF, false);   // Set to Rx continuous mode

	// Request the data
	dwnpkt.type = NET_MTYPE_TRSMT_DATA;
	dwnpkt.payloadlen = 8;
	dwnpkt.payload[0] = (NumReqBytes >> 24) & 0xFF;
	dwnpkt.payload[1] = (NumReqBytes >> 16) & 0xFF;
	dwnpkt.payload[2] = (NumReqBytes >> 8) & 0xFF;
	dwnpkt.payload[3] = NumReqBytes & 0xFF;
	dwnpkt.payload[4] = (ReqByteOffset >> 24) & 0xFF;
	dwnpkt.payload[5] = (ReqByteOffset >> 16) & 0xFF;
	dwnpkt.payload[6] = (ReqByteOffset >> 8) & 0xFF;
	dwnpkt.payload[7] = ReqByteOffset & 0xFF;

	len = ConstructNetPacket(buff, NET_PACKET_MAXLEN, &dwnpkt);

	LAMBDA62_ClearIRQ(&hspi3_rf, 0xFFFF, false);
	LAMBDA62_SetPacketParamsFSK(&hspi3_rf, 32, 5, 64, 0, true, len, 2, false, false);

	xSemaphoreTake(LAMBDA62TxSemphr, 0);   // Clear any spurious Tx notifications
	LAMBDA62_SendPacket(&hspi3_rf, buff, len, false);
	xSemaphoreGive(SPIRfMutex);

	// Wait for Tx to finish before continuing
	if (xSemaphoreTake(LAMBDA62TxSemphr, pdMS_TO_TICKS(100)) != pdTRUE) {
		printf("[ERROR] Data request Tx timed out\n");
		xTimerReset(DiscoveryTimer, 0);   // Restart discovery calls
		return TM_STATE_IDLE;
	}

	if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(20)) != pdTRUE) {
		printf("[ERROR] L62 not reset after discovery Tx due to unreleased SPI mutex\n");
		xTimerReset(DiscoveryTimer, 0);   // Restart discovery calls
		return TM_STATE_IDLE;
	}

	// Clear Tx interrupt
	LAMBDA62_ClearIRQ(&hspi3_rf, 0xFFFF, false);
	xSemaphoreGive(SPIRfMutex);


	// Loop to receive and relay packets. End if there is a timeout or all bytes are received
	TickType_t rectime = xTaskGetTickCount();
	bool done = false;
	while ((xTaskGetTickCount() - rectime) < pdMS_TO_TICKS(500)) {
		if (xQueueReceive(RadioResponseQueue, resp, pdMS_TO_TICKS(10)) == pdPASS) {
			if (resp->type != NET_MTYPE_TRSMT_DATA) { continue; }
			rectime = xTaskGetTickCount();   // Reset timeout window

			// Relay to host
			relaypkt.payloadlen = resp->payloadlen;

			for (int i = 0; i < resp->payloadlen; i++) {   // Copy over received data values
				relaypkt.payload[i] = resp->payload[i];
			}

			SendPacketUSB(&relaypkt);

			// Get sequence number
			uint32_t seqnum = ((uint32_t)resp->payload[0] << 24) | ((uint32_t)resp->payload[1] << 16) |
							  ((uint32_t)resp->payload[2] << 8) | resp->payload[3];

			if ((seqnum + 1) * USB_PAYLOAD_MAXLEN >= NumReqBytes) {   // Quick calculation of received bytes based on seqnum
				done = true;
				break;
			}
		}
	}

	if (done) {
		printf("[INFO] Data request finished succesfully\n");
	} else {
		printf("[ERROR] Data request Rx timed out\n");
	}

	// Switch back to default telemetry modulation settings
	if (xSemaphoreTake(SPIRfMutex, pdMS_TO_TICKS(20)) != pdTRUE) {
		printf("[ERROR] L80 not returned to telemetry mode after data download due to unreleased SPI mutex\n");
		xTimerReset(DiscoveryTimer, 0);   // Restart discovery calls
		return TM_STATE_IDLE;
	}

	LAMBDA80_SetMode_Telemetry(&hspi3_rf, false);
	xSemaphoreGive(SPIRfMutex);

	xTimerReset(DiscoveryTimer, 0);   // Restart discovery calls

	return TM_STATE_IDLE;
}




int main(void) {
	// System init
    HAL_Init();

	SystemClockConfig();
	InitialiseGPIO();
	InitialiseSPI();
	InitialiseTimers();

//	HAL_Delay(2000);   // Startup delay to avoid code executing inbetween debug sessions

	__enable_irq();

	// Initialise RF modules
	InitialiseLAMBDA62FSK(&hspi3_rf, true);
	InitialiseLAMBDA80(&hspi3_rf, true);
	LAMBDA80_SetMode_Telemetry(&hspi3_rf, true);

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

    DiscoveryTimer = xTimerCreate("DiscoveryTimer", pdMS_TO_TICKS(4000), pdTRUE, (void *)0, SendDiscoveryPacket);
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
    if (xSemaphoreTake(USBTxMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
    	printf("[ERROR] USB write timed out\n");
    	return;
    }

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
	hspi3_rf.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;

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



