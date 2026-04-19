#ifndef W25Q_H_
#define W25Q_H_

#include <stdbool.h>
#include <stdint.h>
#include "datatypes.h"


uint32_t FirstFreeAddr = 0;   // First byte address where data is not currently written
uint8_t FirstFreePagePosition = 0;   // Position of the first unprogrammed byte in page


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
#define W25Q_BLOCK_ERASE_4BADDR 0xD8U
#define W25Q_SECTOR_ERASE_4BADDR 0x21U

#define W25Q_PAGE_PROG_4BADDR 0x12U

#define W25Q_READ_4BADDR 0x12U

#define W25Q_READ_STATUS1 0x05U
#define W25Q_READ_STATUS2 0x35U
#define W25Q_READ_STATUS3 0x15U
#define W25Q_WRITE_STATUS1 0x01U
#define W25Q_WRITE_STATUS2 0x31U
#define W25Q_WRITE_STATUS3 0x11U


// Other
#define W25Q_METADATA_ADDR 0x0U
#define W25Q_DATA_ADDR 0x1000U


// Functions
void W25Q_Reset();
bool InitialiseW25Q(bool Erase);

uint32_t W25Q_GetFirstFreePosition();
bool W25Q_CheckBusy();

void W25Q_ReadAll(uint8_t *buff, uint32_t MaxLen);
void W25Q_ReadPart(uint32_t StartAddr, uint8_t *buff, uint32_t ReadLen, uint32_t BuffStartPos);

void W25Q_EnableWrite();

void W25Q_EraseChip();
void W25Q_EraseSector(uint32_t SectorAddr);
void W25Q_EraseBlock(uint32_t BlockAddr);

void W25Q_WriteContiguous(uint8_t *buff, uint32_t Len);
void W25Q_PageProgram(uint32_t Addr, uint8_t *buff, uint32_t Len);



#endif /* W25Q_H_ */
