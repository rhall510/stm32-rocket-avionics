#ifndef W25Q_H_
#define W25Q_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "datatypes.h"


// Global variables to store metadata info
extern uint32_t MetaStartAddr;   // Start address of the metadata sector (1 indicates empty start address)
extern uint32_t MetaEndAddr;   // End address of the metadata sector (1 indicates empty end address)
extern uint32_t DataStartAddr;   // Start address of the flight data block
extern uint32_t DataEndAddr;   // Last byte address of the flight data block
extern uint8_t BadBlocks[64];   // Bytes represent 8 contiguous blocks with lowest address at LSB and highest at MSB. 1 if bad, 0 if ok.


// MCU connected pins
#define W25Q_CS_PORT GPIOC
#define W25Q_CS_PIN GPIO_PIN_6

#define W25Q_WP_PORT GPIOC
#define W25Q_WP_PIN GPIO_PIN_7   // MUST BE HELD HIGH TO ALLOW WRITES TO STATUS REGISTER

#define W25Q_HOLD_PORT GPIOB
#define W25Q_HOLD_PIN GPIO_PIN_12   // MUST BE HELD HIGH TO ALLOW COMMUNICATIONS


// Instructions
#define W25Q_ENABLE_RESET 0x66U
#define W25Q_RESET 0x99U

#define W25Q_JEDECID 0x9FU

#define W25Q_WRITE_ENABLE 0x06U
#define W25Q_WRITE_DISABLE 0x04U

#define W25Q_4BYTE_ADDR_MODE 0xB7U

#define W25Q_CHIP_ERASE 0xC7U
#define W25Q_BLOCK_ERASE_4BADDR 0xDCU
#define W25Q_SECTOR_ERASE_4BADDR 0x21U

#define W25Q_PAGE_PROG_4BADDR 0x12U

#define W25Q_READ_4BADDR 0x13U

#define W25Q_READ_STATUS1 0x05U
#define W25Q_READ_STATUS2 0x35U
#define W25Q_READ_STATUS3 0x15U
#define W25Q_WRITE_STATUS1 0x01U
#define W25Q_WRITE_STATUS2 0x31U
#define W25Q_WRITE_STATUS3 0x11U


// Other
#define W25Q_METADATA_ADDR 0x0U
#define W25Q_DATA_ADDR 0x1000U

#define W25Q_META_START_ID 0x1122334455667788ULL
#define W25Q_DATA_SYNC_WORD 0xAABBBBAAU

#define W25Q_MAX_ADDR 0x01FFFFFFU

#define W25Q_SPI_MAX_DELAY 50


// Functions
void W25Q_Reset();
bool InitialiseW25Q();

bool W25Q_CheckBusy();
void W25Q_EnableWrite();



/* Check the address is safe to write to (i.e. not in a bad block or in the metadata/data sections), if not return the closest
 * safe address downstream. Also checks if the address extends beyond the end of memory and wraps it around.
 */
uint32_t W25Q_GetSafeContiguousWriteAddress(uint32_t Addr);

/* Version of W25Q_GetSafeContiguousWriteAddress that does not move the return address outside of data sectors.
 * Used for reading flight data packets.
 */
uint32_t W25Q_GetSafeContiguousReadAddress(uint32_t Addr);

/* Wrapper around W25Q_GetSafeContiguousReadAddress that ensures an equivalent length of safe space is left before the return address
 * as there was blocked space between StartAddr and StartAddr + Offset. Used for traversing the data section by only reading the headers.
 */
uint32_t W25Q_GetSafeContiguousReadAddressWithOffset(uint32_t StartAddr, uint32_t Offset);

// Returns the first valid address for the metadata sector, or 1 if no valid address can be found
uint32_t W25Q_GetSafeMetadataAddress();


// Find and read metadata sector. Returns true if the sector was sucessfully found, false if a blank one was created
bool W25Q_ReadMetadata();

// Writes metadata to the first available sector
void W25Q_WriteMetadata(bool ErasePrev);


// Reads a contiguous volume of data page by page into the buffer. Performs no checks for safe addresses
void W25Q_ReadVolume(uint32_t StartAddr, uint8_t *buff, uint32_t MaxLen);

// Version of W25Q_ReadVolume which only prints out data a page at a time (skips copying the data over to a buffer)
void W25Q_OutputVolume(uint32_t StartAddr, uint32_t MaxLen);

// Version of W25Q_ReadVolume which skips bad blocks and metadata
void W25Q_ReadVolumeSafe(uint32_t StartAddr, uint8_t *buff, uint32_t MaxLen);

// Version of W25Q_ReadVolumeSafe which only prints out data a page at a time (skips copying the data over to a buffer)
void W25Q_OutputVolumeSafe(uint32_t StartAddr, uint32_t MaxLen);


// Erase parts of memory
void W25Q_EraseChip();
void W25Q_EraseSector(uint32_t SectorAddr);
void W25Q_EraseBlock(uint32_t BlockAddr);

// Erases all sectors with any flight data written to them
void W25Q_EraseFlightData();


// Writes data to the end of the flight data section
void W25Q_WriteAppendData(uint8_t *buff, uint32_t Len);

// Writes data to a contiguous block of memory, skipping bad blocks and the metadata sector. Returns the end address of data written
// Will not check the write address if CheckAddress == true
uint32_t W25Q_WriteVolume(uint32_t StartAddr, uint8_t *buff, uint32_t Len, bool CheckAddress);

// Writes data to a single page, failing if the write would overflow
bool W25Q_PageProgram(uint32_t StartAddr, uint8_t *buff, uint16_t Len);


// Scans and records all bad blocks in memory. WARNING: will erase all data and may take a while to complete
void W25Q_ScanBadBlocks();



// Testing functions
// Tests initialisation and reading of existing data on the chip
void Test_W25Q_Initialisation();

// Tests erasure of previous flight data and logging of two small packets
void Test_W25Q_Logging();

// Tests bad block and metadata sector management when writing and reading, as well as scanning for bad blocks
void Test_W25Q_BadBlocks();

// Tests wraparound behaviour at the end of memory
void Test_W25Q_Wraparound();


#endif /* W25Q_H_ */
