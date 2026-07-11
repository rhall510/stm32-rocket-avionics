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
	MAX10S_Reset(hi2c, Blocking);

	if (HAL_I2C_IsDeviceReady(hi2c, M10S_I2C_ADDR, 3, 100) != HAL_OK) {
		return false;
	}

	// Command string generated with u-center
	uint8_t confcmd[] = {
		0xB5, 0x62, 0x06, 0x8A, 0x28, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x21, 0x00, 0x11, 0x20, 0x08, 0x01,
		0x00, 0x21, 0x30, 0x64, 0x00, 0xC9, 0x00, 0x91,
		0x20, 0x00, 0xBF, 0x00, 0x91, 0x20, 0x00, 0xC4,
		0x00, 0x91, 0x20, 0x00, 0xB0, 0x00, 0x91, 0x20,
		0x00, 0x01, 0x00, 0xD0, 0x20, 0x00, 0x7A, 0x97
	};

	MAXM10S_SendCommand(hi2c, confcmd, sizeof(confcmd));

	MAX10S_SetSleep(hi2c);

	return true;
}


void MAX10S_SetSleep(I2C_HandleTypeDef *hi2c) {
	MAXM10S_FlushBuffer(hi2c);

	// Set to software backup mode with infinite duration and default wakeup sources
	uint8_t payload[16] = {0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0};
	MAXM10S_SendUBX(hi2c, 0x02, 0x41, payload, 16);
}


void MAX10S_Wake(I2C_HandleTypeDef *hi2c) {
	// Execute any I2C command to recover from software backup mode
	MAXM10S_GetAvailableBytes(hi2c);
}


void MAX10S_Reset(I2C_HandleTypeDef *hi2c, bool Blocking) {
	HAL_GPIO_WritePin(M10S_RST_PORT, M10S_RST_PIN, GPIO_PIN_RESET);

	if (Blocking) {
		HAL_Delay(2);
	} else {
		vTaskDelay(pdMS_TO_TICKS(2));
	}

	HAL_GPIO_WritePin(M10S_RST_PORT, M10S_RST_PIN, GPIO_PIN_SET);

	if (Blocking) {
		HAL_Delay(50);
	} else {
		vTaskDelay(pdMS_TO_TICKS(50));
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


void MAXM10S_ProcessNMEASentence(const char *sentence, TS_GPS *data) {
	int type = minmea_sentence_id(sentence, false);

	if (type == MINMEA_SENTENCE_RMC) {
		struct minmea_sentence_rmc frame;
		if (minmea_parse_rmc(&frame, sentence)) {
			data->HasFix = frame.valid;

			if (frame.valid) {
				// Convert to standard floats
				data->Latitude = minmea_tocoord(&frame.latitude);
				data->Longitude = minmea_tocoord(&frame.longitude);
				data->Speed = minmea_tofloat(&frame.speed);

				data->Timestamp = (float)HAL_GetTick() / 1000.0f;
			}
		}
	} else if (type == MINMEA_SENTENCE_GGA) {
		struct minmea_sentence_gga frame;
		if (minmea_parse_gga(&frame, sentence)) {
			if (frame.fix_quality > 0) {
				data->Altitude = minmea_tofloat(&frame.altitude);
				data->Satellites = frame.satellites_tracked;
			}
		}
	}
}


bool MAXM10S_ParseStream(uint8_t *i2c_data, uint16_t length, TS_GPS *data) {
	bool SentenceFound = false;

	for (uint16_t i = 0; i < length; i++) {
		char c = (char)i2c_data[i];

		// Fill buffer
		if (line_len < M10S_LINE_BUFFER_SIZE - 1) {
			line_buffer[line_len++] = c;
		}

		// Wait for end of a sentence (\n)
		if (c == '\n') {
			line_buffer[line_len] = '\0';   // Add null terminator
			MAXM10S_ProcessNMEASentence(line_buffer, data);
			SentenceFound = true;
			line_len = 0;
		}
	}

	return SentenceFound;
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
		buff[*BuffPos] = 5 * sizeof(float) + 2;
		*BuffPos += 1;

		memcpy(buff + *BuffPos, &databuff[i].Timestamp, sizeof(float));
		*BuffPos += sizeof(float);
		memcpy(buff + *BuffPos, &databuff[i].Latitude, sizeof(float));
		*BuffPos += sizeof(float);
		memcpy(buff + *BuffPos, &databuff[i].Longitude, sizeof(float));
		*BuffPos += sizeof(float);
		memcpy(buff + *BuffPos, &databuff[i].Altitude, sizeof(float));
		*BuffPos += sizeof(float);
		memcpy(buff + *BuffPos, &databuff[i].Speed, sizeof(float));
		*BuffPos += sizeof(float);
		buff[*BuffPos] = databuff[i].Satellites;
		*BuffPos += 1;
		buff[*BuffPos] = databuff[i].HasFix;
		*BuffPos += 1;
	}

	return true;
}









