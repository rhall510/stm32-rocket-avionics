#include "stm32g4xx.h"
#include "m10s.h"


char line_buffer[M10S_LINE_BUFFER_SIZE];
uint8_t line_len = 0;


uint16_t MAXM10S_GetAvailableBytes(I2C_HandleTypeDef *hi2c) {
    uint8_t buff[2] = {0};

    if (HAL_I2C_Mem_Read(hi2c, M10S_I2C_ADDR, M10S_AVAIL, I2C_MEMADD_SIZE_8BIT, buff, 2, HAL_MAX_DELAY) == HAL_OK) {
        return (uint16_t)((buff[0] << 8) | buff[1]);
    }
    return 0;
}


bool MAXM10S_ReadStream(I2C_HandleTypeDef *hi2c, uint8_t *buffer, uint16_t length) {
    if (length == 0) return false;

    if (HAL_I2C_Mem_Read(hi2c, M10S_I2C_ADDR, M10S_DATA, I2C_MEMADD_SIZE_8BIT, buffer, length, HAL_MAX_DELAY) == HAL_OK) {
        return true;
    }
    return false;
}


bool MAXM10S_SendCommand(I2C_HandleTypeDef *hi2c, uint8_t *cmd, uint16_t length) {
    if (length < 2) return false;   // Requires >= 2 bytes for a command write

    if (HAL_I2C_Master_Transmit(hi2c, M10S_I2C_ADDR, cmd, length, HAL_MAX_DELAY) == HAL_OK) {
        return true;
    }
    return false;
}


bool MAXM10S_SendUBX(I2C_HandleTypeDef *hi2c, uint8_t class, uint8_t id, uint8_t *payload, uint16_t len) {
    uint8_t packet[len + 8];

    // Header
    packet[0] = 0xB5;
    packet[1] = 0x62;
    packet[2] = class;
    packet[3] = id;
    packet[4] = (uint8_t)(len & 0xFF);
    packet[5] = (uint8_t)(len >> 8);

    // Payload
    if (len > 0) {
        memcpy(&packet[6], payload, len);
    }

    // Checksum calculation spanning class, id, length, and payload
    uint8_t ck_a = 0, ck_b = 0;
    for (int i = 2; i < len + 6; i++) {
        ck_a += packet[i];
        ck_b += ck_a;
    }

    packet[6 + len] = ck_a;
    packet[7 + len] = ck_b;

    return MAXM10S_SendCommand(hi2c, packet, len + 8);
}


bool InitialiseMAXM10S(I2C_HandleTypeDef *hi2c, bool Blocking) {
	MAXM10S_Reset(hi2c, Blocking);

	if (HAL_I2C_IsDeviceReady(hi2c, M10S_I2C_ADDR, 3, 100) != HAL_OK) {
		return false;
	}

	// Command string generated with u-center
	// 2Hz DR, AIR4 model, no GLL, GSA, GSV, VTG, GGA, RMC messages (start with no data output)
	uint8_t confcmd[] = {
	    0xB5, 0x62, 0x06, 0x8A, 0x32, 0x00, 0x00, 0x01,
	    0x00, 0x00, 0x21, 0x00, 0x11, 0x20, 0x08, 0x01,
	    0x00, 0x21, 0x30, 0xF4, 0x01, 0xC9, 0x00, 0x91,
	    0x20, 0x00, 0xBF, 0x00, 0x91, 0x20, 0x00, 0xC4,
	    0x00, 0x91, 0x20, 0x00, 0xB0, 0x00, 0x91, 0x20,
	    0x00, 0x01, 0x00, 0xD0, 0x20, 0x00, 0xAB, 0x00,
	    0x91, 0x20, 0x00, 0xBA, 0x00, 0x91, 0x20, 0x00,
	    0xDC, 0x02
	};

	MAXM10S_SendCommand(hi2c, confcmd, sizeof(confcmd));

	return true;
}


void MAXM10S_SetSleep(I2C_HandleTypeDef *hi2c) {
	MAXM10S_FlushBuffer(hi2c);

	// Set to software backup mode with infinite duration and default wakeup sources
	uint8_t payload[16] = {0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0};
	MAXM10S_SendUBX(hi2c, 0x02, 0x41, payload, 16);
}


void MAXM10S_Wake(I2C_HandleTypeDef *hi2c) {
	// Execute any I2C command to recover from software backup mode
	MAXM10S_GetAvailableBytes(hi2c);
}


void MAXM10S_SetDataOutput(I2C_HandleTypeDef *hi2c, bool enable, bool Blocking) {
    if (enable) {
        // Enable PVT messages
        uint8_t cmd[] = {
        	0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x01,
			0x00, 0x00, 0x06, 0x00, 0x91, 0x20, 0x01, 0x52, 0x43
        };

        MAXM10S_SendCommand(hi2c, cmd, sizeof(cmd));

		if (Blocking) {   // Delay to allow command processing
			HAL_Delay(5);
		} else {
			vTaskDelay(pdMS_TO_TICKS(5));
		}

		// Flush the buffer to ensure it starts clean
        MAXM10S_FlushBuffer(hi2c);
    } else {
        // Disable PVT messages
        uint8_t cmd[] = {
        	0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x01,
            0x00, 0x00, 0x06, 0x00, 0x91, 0x20, 0x00, 0x51, 0x42
        };

        MAXM10S_SendCommand(hi2c, cmd, sizeof(cmd));
    }
}


void MAXM10S_Reset(I2C_HandleTypeDef *hi2c, bool Blocking) {
	HAL_GPIO_WritePin(M10S_RST_PORT, M10S_RST_PIN, GPIO_PIN_RESET);

	if (Blocking) {
		HAL_Delay(2);
	} else {
		vTaskDelay(pdMS_TO_TICKS(2));
	}

	HAL_GPIO_WritePin(M10S_RST_PORT, M10S_RST_PIN, GPIO_PIN_SET);

	if (Blocking) {
		HAL_Delay(200);
	} else {
		vTaskDelay(pdMS_TO_TICKS(200));
	}
}


void MAXM10S_FlushBuffer(I2C_HandleTypeDef *hi2c) {
    uint16_t avail = MAXM10S_GetAvailableBytes(hi2c);
    uint8_t buff[100];

    // Keep reading until the buffer is empty
    while (avail > 0) {
        uint16_t readnum = (avail > sizeof(buff)) ? sizeof(buff) : avail;

        MAXM10S_ReadStream(hi2c, buff, readnum);
        avail = MAXM10S_GetAvailableBytes(hi2c);
    }
}


bool MAXM10S_ParseUBXStream(uint8_t *i2c_data, uint16_t length, uint16_t *readoffset, UBXPacket *pkt) {
	static UBXParserState state = UBXPARSER_WAIT_SYNC1;
	static uint16_t payloadidx = 0;

	static uint8_t chka;
	static uint8_t chkb;

	for (uint16_t i = *readoffset; i < length; i++) {
		*readoffset = i + 1;   // Update readoffset to point to the first unparsed byte

		uint8_t byte = i2c_data[i];

		switch (state) {
			case UBXPARSER_WAIT_SYNC1:
				if (byte == (UBX_SYNC_WORD >> 8)) {
					state = UBXPARSER_WAIT_SYNC2;
				}
				break;

			case UBXPARSER_WAIT_SYNC2:
				if (byte == (UBX_SYNC_WORD & 0xFF)) {
					state = UBXPARSER_CLASS;
				} else if (byte != (UBX_SYNC_WORD >> 8)) {
					state = UBXPARSER_WAIT_SYNC1;
				}
				break;

			case UBXPARSER_CLASS:
				pkt->class = byte;
				state = UBXPARSER_ID;
				break;

			case UBXPARSER_ID:
				pkt->id = byte;
				state = UBXPARSER_LEN1;
				break;

			case UBXPARSER_LEN1:   // LSB first
				pkt->payloadlen = byte;
				state = UBXPARSER_LEN2;
				break;

			case UBXPARSER_LEN2:
				pkt->payloadlen |= ((uint16_t)byte << 8);
				payloadidx = 0;

				if (pkt->payloadlen == 0) {
					state = UBXPARSER_CHKSUM1;
				} else {
					state = UBXPARSER_PAYLOAD;
				}

				break;

			case UBXPARSER_PAYLOAD:
				pkt->payload[payloadidx++] = byte;

				if (payloadidx >= pkt->payloadlen) {
					state = UBXPARSER_CHKSUM1;
				} else if (payloadidx >= UBX_PAYLOAD_MAXLEN) {
					printf("[ERROR] UBX parser found longer than allowed payload\n");
					state = UBXPARSER_WAIT_SYNC1;
				}

				break;

			case UBXPARSER_CHKSUM1:
				chka = byte;
				state = UBXPARSER_CHKSUM2;
				break;

			case UBXPARSER_CHKSUM2:
				chkb = byte;

				// Verify checksum
				uint8_t ck_a = 0, ck_b = 0;

			    ck_a += pkt->class;
			    ck_b += ck_a;
			    ck_a += pkt->id;
			    ck_b += ck_a;
			    ck_a += (pkt->payloadlen & 0xFF);
			    ck_b += ck_a;
			    ck_a += (pkt->payloadlen >> 8);
			    ck_b += ck_a;

				for (int i = 0; i < pkt->payloadlen; i++) {
				    ck_a += pkt->payload[i];
				    ck_b += ck_a;
				}

				if (ck_a == chka && ck_b == chkb) {
					state = UBXPARSER_WAIT_SYNC1;
					return true;
				} else {
					state = UBXPARSER_WAIT_SYNC1;
				}

				break;
		}
	}

	return false;
}


void MAXM10S_ExtractPVTData(UBXPacket *pkt, TS_GPS *data) {
	UBX_NAV_PVT *pvt = (UBX_NAV_PVT *)pkt->payload;

	data->Timestamp = (float)HAL_GetTick() / 1000.0f;

	data->Latitude = pvt->lat * 1e-7;
	data->Longitude = pvt->lon * 1e-7;
	data->Altitude = pvt->hMSL * 1e-3;

	data->VelNorth = pvt->velN * 1e-3;
	data->VelEast = pvt->velE * 1e-3;
	data->VelDown = pvt->velD * 1e-3;
	data->GroundSpeed = pvt->gSpeed * 1e-3;

	data->Heading = pvt->headMot * 1e-5;

	data->HorzAccuracy = pvt->hAcc * 1e-3;
	data->VertAccuracy = pvt->vAcc * 1e-3;
	data->SpeedAccuracy = pvt->sAcc * 1e-3;

	data->Satellites = pvt->numSV;
	data->FixType = pvt->fixType;
}


bool MAXM10S_AppendLogPacket(uint8_t *buff, uint16_t *BuffPos, uint16_t BuffMaxLen, TS_GPS *databuff, uint8_t Readings) {
	// Check the data can fit in the buffer
	uint16_t ByteLen = Readings * M10S_PKT_DATA_LEN;

	if (BuffMaxLen - *BuffPos < ByteLen) {
		printf("WARNING: Could not write MAXM10S data to write buffer due to lack of space");
		return false;
	}

	// Copy data into the buffer
	for (int i = 0; i < Readings; i++) {
		buff[*BuffPos] = 0b01010000;   // Type 5 packet (GPS)
		*BuffPos += 1;
		buff[*BuffPos] = M10S_PKT_DATA_LEN - 2;
		*BuffPos += 1;

		memcpy(buff + *BuffPos, &databuff[i], M10S_PKT_DATA_LEN - 2);
		*BuffPos += M10S_PKT_DATA_LEN - 2;
	}

	return true;
}









