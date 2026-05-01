#ifndef M10S_H_
#define M10S_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "datatypes.h"
#include "minmea.h"


// MCU connected pins
#define M10S_RST_PORT GPIOA
#define M10S_RST_PIN GPIO_PIN_12


// Registers
#define M10S_AVAIL 0xFDU
#define M10S_DATA 0xFFU


// Other
#define M10S_I2C_ADDR (0x42 << 1)
#define M10S_LINE_BUFFER_SIZE 128
#define M10S_PKT_DATA_LEN (4 + 5 * sizeof(float))


// Functions
void InitialiseMAXM10S();

bool MAXM10S_SendCommand(uint8_t *cmd, uint16_t length);

uint16_t MAXM10S_GetAvailableBytes();
bool MAXM10S_ReadStream(uint8_t *buffer, uint16_t length);

// Process NMEA sentences in the buffer and update the data struct
void MAXM10S_ProcessNMEASentence(const char *sentence, struct TS_GPS *data);

// Parse the raw data stream and call MAXM10S_ProcessNMEASentence when a full sentence is constructed. Returns true if a sentence is found
bool MAXM10S_ParseStream(uint8_t *i2c_data, uint16_t length, struct TS_GPS *data);

// Constructs a flash logging packet from the given data and appends it to the buffer
bool MAXM10S_AppendLogPacket(uint8_t *buff, uint16_t *BuffPos, uint16_t BuffMaxLen, volatile struct TS_GPS *databuff, uint8_t Readings);

#endif /* M10S_H_ */
