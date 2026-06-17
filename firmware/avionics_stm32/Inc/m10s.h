#ifndef M10S_H_
#define M10S_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "datatypes.h"
#include "minmea.h"
#include "pinconfig.h"


// Registers
#define M10S_AVAIL 0xFDU
#define M10S_DATA 0xFFU


// Other
#define M10S_I2C_ADDR (0x42 << 1)
#define M10S_LINE_BUFFER_SIZE 128
#define M10S_PKT_DATA_LEN (4 + 5 * sizeof(float))


// Functions
bool InitialiseMAXM10S();

// Set module into software backup mode (no measurements taken) and clear fifo buffer
void MAX10S_SetSleep();

// Wake module from software backup mode and clear fifo buffer
void MAX10S_Wake();

// Hardware reset
void MAX10S_Reset();

// Flush all available bytes out of the I2C buffer
void MAXM10S_FlushBuffer();

// Send a raw byte array over I2C
bool MAXM10S_SendCommand(uint8_t *cmd, uint16_t length);

// Construct and send a UBX command
bool MAXM10S_SendUBX(uint8_t class, uint8_t id, uint8_t *payload, uint16_t len);

uint16_t MAXM10S_GetAvailableBytes();
bool MAXM10S_ReadStream(uint8_t *buffer, uint16_t length);

// Process NMEA sentences in the buffer and update the data struct
void MAXM10S_ProcessNMEASentence(const char *sentence, TS_GPS *data);

// Parse the raw data stream and call MAXM10S_ProcessNMEASentence when a full sentence is constructed. Returns true if a sentence is found
bool MAXM10S_ParseStream(uint8_t *i2c_data, uint16_t length, TS_GPS *data);

// Constructs a flash logging packet from the given data and appends it to the buffer
bool MAXM10S_AppendLogPacket(uint8_t *buff, uint16_t *BuffPos, uint16_t BuffMaxLen, volatile TS_GPS *databuff, uint8_t Readings);

#endif /* M10S_H_ */
