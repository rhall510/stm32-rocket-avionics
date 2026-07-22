#ifndef M10S_H_
#define M10S_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "datatypes.h"
#include "minmea.h"
#include "pinconfig.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32g4xx.h"


// Registers
#define M10S_AVAIL 0xFDU
#define M10S_DATA 0xFFU


// Other
#define M10S_I2C_ADDR (0x42 << 1)
#define M10S_LINE_BUFFER_SIZE 128
#define M10S_PKT_DATA_LEN (2 + sizeof(TS_GPS))


#define UBX_SYNC_WORD 0xB562U

typedef enum {
    UBXPARSER_WAIT_SYNC1,
	UBXPARSER_WAIT_SYNC2,
	UBXPARSER_CLASS,
	UBXPARSER_ID,
	UBXPARSER_LEN1,
	UBXPARSER_LEN2,
	UBXPARSER_PAYLOAD,
	UBXPARSER_CHKSUM1,
	UBXPARSER_CHKSUM2
} UBXParserState;


#define UBX_PAYLOAD_MAXLEN 100

typedef struct {
	uint8_t class;
	uint8_t id;
	uint16_t payloadlen;
	uint8_t payload[UBX_PAYLOAD_MAXLEN];
} UBXPacket;


// Struct to unpack PVT binary payload
typedef struct __attribute__((packed)) {
    uint32_t iTOW;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    uint8_t valid;
    uint32_t tAcc;
    int32_t nano;
    uint8_t fixType;
    uint8_t flags;
    uint8_t flags2;
    uint8_t numSV;
    int32_t lon;
    int32_t lat;
    int32_t height;
    int32_t hMSL;
    uint32_t hAcc;
    uint32_t vAcc;
    int32_t velN;
    int32_t velE;
    int32_t velD;
    int32_t gSpeed;
    int32_t headMot;
    uint32_t sAcc;
    uint32_t headAcc;
    uint16_t pDOP;
    uint16_t flags3;
    uint32_t reserved1;
    int32_t headVeh;
    int16_t magDec;
    uint16_t magAcc;
} UBX_NAV_PVT;


// Functions
// Initialises the MAXM10S and immediately sets it to sleep mode so no measurements are taken
bool InitialiseMAXM10S(I2C_HandleTypeDef *hi2c, bool Blocking);

// Clear fifo buffer and set module into software backup mode (no measurements taken)
void MAXM10S_SetSleep(I2C_HandleTypeDef *hi2c);

// Wake module from software backup mode
void MAXM10S_Wake(I2C_HandleTypeDef *hi2c);

// Enable/disable output of GGA and RMC NMEA sentences over I2C
void MAXM10S_SetDataOutput(I2C_HandleTypeDef *hi2c, bool enable, bool Blocking);

// Hardware reset
void MAXM10S_Reset(I2C_HandleTypeDef *hi2c, bool Blocking);

// Flush all available bytes out of the I2C buffer
void MAXM10S_FlushBuffer(I2C_HandleTypeDef *hi2c);

// Send a raw byte array over I2C
bool MAXM10S_SendCommand(I2C_HandleTypeDef *hi2c, uint8_t *cmd, uint16_t length);

// Construct and send a UBX command
bool MAXM10S_SendUBX(I2C_HandleTypeDef *hi2c, uint8_t class, uint8_t id, uint8_t *payload, uint16_t len);

uint16_t MAXM10S_GetAvailableBytes(I2C_HandleTypeDef *hi2c);
bool MAXM10S_ReadStream(I2C_HandleTypeDef *hi2c, uint8_t *buffer, uint16_t length);

// Parse the raw data stream for binary packets and updates the provided pkt struct.
// Exits early and returns true if a full packet is found. Readoffset is updated to point to the first unread byte of the input data
// So the calling function can continue parsing after processing the current packet
bool MAXM10S_ParseUBXStream(uint8_t *i2c_data, uint16_t length, uint16_t *readoffset, UBXPacket *pkt);

// Take a PVT UBX binary packet and parse it into a TS_GPS struct
void MAXM10S_ExtractPVTData(UBXPacket *pkt, TS_GPS *data);

// Constructs a flash logging packet from the given data and appends it to the buffer
bool MAXM10S_AppendLogPacket(uint8_t *buff, uint16_t *BuffPos, uint16_t BuffMaxLen, TS_GPS *databuff, uint8_t Readings);

#endif /* M10S_H_ */
