#include "w25q.h"
#include "stm32g4xx.h"


extern SPI_HandleTypeDef hspi2_str;


// Global variables to store metadata info
uint32_t MetaStartAddr = 1;   // Start address of the metadata sector (1 indicates empty start address)
uint32_t MetaEndAddr = 1;   // End address of the metadata sector (1 indicates empty end address)
uint32_t DataStartAddr = 0;   // Start address of the flight data block
uint32_t DataEndAddr = 0;   // Last byte address of the flight data block
uint8_t BadBlocks[64] = {0};   // Bytes represent 8 contiguous blocks with lowest address at LSB and highest at MSB. 1 if bad, 0 if ok.


bool InitialiseW25Q() {
	W25Q_Reset();

	// Check device ID
	uint8_t tx[4] = {0};
	uint8_t rx[4] = {0};

	tx[0] = W25Q_JEDECID;

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(&hspi2_str, tx, rx, 4, W25Q_SPI_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	if (rx[1] != 0xEF || (((uint16_t)rx[2] << 8) + rx[3]) != 0x4019) {
		return false;
	}

	// Set to 4 byte address mode
	tx[0] = W25Q_4BYTE_ADDR_MODE;

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2_str, tx, 1, W25Q_SPI_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	W25Q_ReadMetadata();

	return true;
}


void W25Q_Reset() {
	// Enable software reset
	uint8_t tx[1] = {W25Q_ENABLE_RESET};

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2_str, tx, 1, W25Q_SPI_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	// Execute software reset
	tx[0] = W25Q_RESET;

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2_str, tx, 1, W25Q_SPI_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	HAL_Delay(1);   // Must wait at least 30uS
}


bool W25Q_CheckBusy() {
	// Read status 1 register
	uint8_t tx[2] = {W25Q_READ_STATUS1, 0};
	uint8_t rx[2] = {0};

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(&hspi2_str, tx, rx, 2, W25Q_SPI_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	return rx[1] & 1;
}


void W25Q_EnableWrite() {
	uint8_t tx[1] = {W25Q_WRITE_ENABLE};

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2_str, tx, 1, W25Q_SPI_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);
}




bool W25Q_ReadMetadata() {
	MetaStartAddr = 1;   // 1 Indicates not found

	uint8_t tx[5] = {0};
	uint8_t rx[68] = {0};   // Allocate enough space for reading full metadata later

	tx[0] = W25Q_READ_4BADDR;

	// Read 8 bytes at the start of each block and check for metadata start identifier
	for (int block = 0; block < 512; block++) {
		uint32_t ReadAddr = (block << 16);
		for (int i = 3; i >= 0; i--) {   // Load block start address into tx
			tx[4 - i] = (uint8_t)((ReadAddr >> (i * 8)) & 0xFF);
		}

		HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
		HAL_SPI_Transmit(&hspi2_str, tx, 5, W25Q_SPI_MAX_DELAY);
		HAL_SPI_Receive(&hspi2_str, rx, 8, W25Q_SPI_MAX_DELAY);
		HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

		bool found = true;
		for (int i = 0; i < 8; i++) {   // Check for metadata start ID
			if (rx[i] != (uint8_t)(W25Q_META_START_ID >> ((7 - i) * 8))) {
				found = false;
				break;
			}
		}

		if (found) {
			MetaStartAddr = ReadAddr;
			printf("Found metadata start at 0x%08lX\n", MetaStartAddr);
			break;
		}
	}

	// Create new metadata block if none found. Must assume no bad blocks and reset addresses to start
	if (MetaStartAddr == 1) {
		printf("Did not find metadata, writing blank...\n");
		MetaStartAddr = 0x0;
		DataStartAddr = 0x1000;
		DataEndAddr = 0x1000;   // No data

		for (int i = 0; i < 64; i++) {   // Clear all bad block flags
			BadBlocks[i] = 0x0;
		}

		W25Q_WriteMetadata(true);
		return false;
	}

	// Read existing metadata if found (4 byte data start address + 64 byte bad block array)
	uint32_t ReadAddr = MetaStartAddr + 8;
	for (int i = 3; i >= 0; i--) {   // Load read address into tx
		tx[4 - i] = (uint8_t)((ReadAddr >> (i * 8)) & 0xFF);
	}

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2_str, tx, 5, W25Q_SPI_MAX_DELAY);
	HAL_SPI_Receive(&hspi2_str, rx, 68, W25Q_SPI_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	DataStartAddr = ((uint32_t)rx[0] << 24) | ((uint32_t)rx[1] << 16) | ((uint32_t)rx[2] << 8) | rx[3];

	printf("Read data start at 0x%08lX\n", DataStartAddr);

	for (int i = 0; i < 64; i++) {   // Store bad block array
		BadBlocks[i] = rx[i + 4];
	}

	MetaEndAddr = MetaStartAddr + 0xFFF;   // Reserve the entire sector for metadata


	// Find data end address
	ReadAddr = DataStartAddr;
	uint32_t LastValidEndAddr = DataStartAddr;
	while (1) {
		// Read 8 bytes at packet start: 4 byte sync word + 4 byte header
		W25Q_ReadVolumeSafe(ReadAddr, rx, 8);

		uint32_t sync = ((uint32_t)rx[0] << 24) | ((uint32_t)rx[1] << 16) | ((uint32_t)rx[2] << 8) | rx[3];
		if (sync == W25Q_DATA_SYNC_WORD) {
			uint32_t packetLen = ((uint16_t)rx[5] << 8) | rx[6];

			// Keep track of the last valid packet end
			LastValidEndAddr = W25Q_GetSafeContiguousReadAddressWithOffset(ReadAddr, packetLen - 1);

			// Increment read address to expected start of the next packet
			ReadAddr = W25Q_GetSafeContiguousReadAddressWithOffset(ReadAddr, packetLen);
		} else {
			DataEndAddr = LastValidEndAddr;
			break;
		}
	}

	printf("Found data end at 0x%08lX\n", DataEndAddr);

	return true;
}


void W25Q_WriteMetadata(bool ErasePrev) {
	if (ErasePrev && MetaStartAddr != 1) {
		W25Q_EraseSector(MetaStartAddr);
	}

	MetaStartAddr = W25Q_GetSafeMetadataAddress();   // Get first valid metadata address
	MetaEndAddr = MetaStartAddr + 0xFFF;      // Reserve the entire sector for metadata

	if (MetaStartAddr == 1) {
		printf("ERROR in W25Q_WriteMetadata: Cannot find valid metadata sector address");
		return;
	}

	uint8_t len = 76;
	uint8_t metadata[len];

	for (int i = 0; i < 8; i++) {   // Start identifier
	    metadata[i] = (uint8_t)(W25Q_META_START_ID >> ((7 - i) * 8));
	}

	for (int i = 0; i < 4; i++) {   // Data start address
	    metadata[i + 8] = (uint8_t)(DataStartAddr >> ((3 - i) * 8));
	}

	for (int i = 0; i < 64; i++) {   // Bad block array
		metadata[i + 12] = BadBlocks[i];
	}

	printf("Writing metadata to 0x%08lX\n", MetaStartAddr);

	W25Q_WriteVolume(MetaStartAddr, metadata, len, false);   // Write without address checking (already checked above)
}



uint32_t W25Q_GetSafeContiguousWriteAddress(uint32_t Addr) {
	uint32_t SafeAddr = Addr;
	bool wrapped = false;

	// Do all checks and recheck if the address moves
	while (1) {
		// Check if the address extends beyond the end of memory
		if (SafeAddr > W25Q_MAX_ADDR) {
			SafeAddr = 0x0;

			if (wrapped) {
				printf("ERROR: Address wraparound twice in W25Q_GetSafeContiguousWriteAddress");
				return 0xFFFFFFFF;
			} else {
				printf("WARNING: Address wraparound in W25Q_GetSafeContiguousWriteAddress");
			}

			wrapped = true;
			continue;
		}

		// Check if the address is in the metadata sector
		if (SafeAddr <= MetaEndAddr && SafeAddr >= MetaStartAddr) {
			printf("WARNING: Address moved out of metadata sector in W25Q_GetSafeContiguousWriteAddress");
			SafeAddr = MetaEndAddr + 1;
			continue;
		}

		// Check if the address is in the data section
		if (DataEndAddr > DataStartAddr) {
			if (SafeAddr <= DataEndAddr && SafeAddr >= DataStartAddr) {
				printf("WARNING: Address moved out of data section in W25Q_GetSafeContiguousWriteAddress");
				SafeAddr = DataEndAddr + 1;
				continue;
			}
		} else if (DataEndAddr < DataStartAddr) {
			if (SafeAddr <= DataEndAddr || SafeAddr >= DataStartAddr) {
				printf("WARNING: Address moved out of data section in W25Q_GetSafeContiguousWriteAddress");
				SafeAddr = DataEndAddr + 1;
				continue;
			}
		}

		// Check if the address is in a bad block
		uint16_t BlockNum = SafeAddr >> 16;
		uint32_t BBStartAddr = SafeAddr & 0xFFFF0000;
		uint32_t BBEndAddr = BBStartAddr + 0xFFFF;

		if (BadBlocks[BlockNum >> 3] & (1U << (BlockNum & 0b111))) {
			SafeAddr = BBEndAddr + 1;
			printf("WARNING: Address moved out of bad block in W25Q_GetSafeContiguousWriteAddress");
			continue;
		}

		break;   // Break if all tests pass
	}

	return SafeAddr;
}


uint32_t W25Q_GetSafeContiguousReadAddress(uint32_t Addr) {
	uint32_t SafeAddr = Addr;

	// Do all checks and recheck if the address moves
	while (1) {
		// Check if the address extends beyond the end of memory
		if (SafeAddr > W25Q_MAX_ADDR) {
			printf("WARNING: Address wraparound in W25Q_GetSafeContiguousReadAddress");
			SafeAddr = 0x0;
			continue;
		}

		// Check if the address is in the metadata sector
		if (SafeAddr <= MetaEndAddr && SafeAddr >= MetaStartAddr) {
			printf("WARNING: Address moved out of metadata sector in W25Q_GetSafeContiguousReadAddress");
			SafeAddr = MetaEndAddr + 1;
			continue;
		}

		// Check if the address is in a bad block
		uint16_t BlockNum = SafeAddr >> 16;
		uint32_t BBStartAddr = SafeAddr & 0xFFFF0000;
		uint32_t BBEndAddr = BBStartAddr + 0xFFFF;

		if (BadBlocks[BlockNum >> 3] & (1U << (BlockNum & 0b111))) {
			SafeAddr = BBEndAddr + 1;
			printf("WARNING: Address moved out of bad block in W25Q_GetSafeContiguousReadAddress");
			continue;
		}

		break;   // Break if all tests pass
	}

	return SafeAddr;
}


uint32_t W25Q_GetSafeContiguousReadAddressWithOffset(uint32_t StartAddr, uint32_t Offset) {
	uint32_t SafeAddr = StartAddr;

	uint16_t PageLeft = 256 - (SafeAddr & 0xFF);
	uint32_t OffsetLeft = Offset;

	if (OffsetLeft >= PageLeft) {
		OffsetLeft -= PageLeft;
		SafeAddr += PageLeft;
	} else {
		SafeAddr += OffsetLeft;
		OffsetLeft = 0;
	}

	while (OffsetLeft > 0) {
		SafeAddr = W25Q_GetSafeContiguousReadAddress(SafeAddr);

		if (OffsetLeft >= 256) {
			OffsetLeft -= 256;
			SafeAddr += 256;
		} else {
			SafeAddr += OffsetLeft;
			OffsetLeft = 0;
		}
	}

	return W25Q_GetSafeContiguousReadAddress(SafeAddr);   // Return with a final safeing check for edge cases
}


uint32_t W25Q_GetSafeMetadataAddress() {
	uint32_t SafeAddr = 0x0;
	bool safe = false;

	uint16_t StartBlock = 0;
	if (MetaStartAddr != 1) {   // Start searching from the block immediately after the current metadata
		StartBlock = (MetaStartAddr >> 16) + 1;
	}

	// Check each block start address
	for (int i = 0; i < 512; i++) {
		uint16_t block = (StartBlock + i) % 512;   // Wrap block around to the beginning of memory
		SafeAddr = (block << 16);

		// Check if the address is in the data section
		if (DataEndAddr > DataStartAddr) {
			if (SafeAddr <= DataEndAddr && SafeAddr >= DataStartAddr) { continue; }
		} else if (DataEndAddr < DataStartAddr) {
			if (SafeAddr <= DataEndAddr || SafeAddr >= DataStartAddr) { continue; }
		}

		// Check if the address is in a bad block
		uint16_t BlockNum = SafeAddr >> 16;
		if (BadBlocks[BlockNum >> 3] & (1U << (BlockNum & 0b111))) { continue; }

		safe = true;
		break;   // Break if all tests pass
	}

	return safe ? SafeAddr : 1;
}




void W25Q_WriteAppendData(uint8_t *buff, uint32_t Len) {
	// Start exactly at the start address if memory is empty
	uint32_t NextWriteAddr = (DataStartAddr == DataEndAddr) ? DataStartAddr : DataEndAddr + 1;
	DataEndAddr = W25Q_WriteVolume(NextWriteAddr, buff, Len, true);

	printf("Flight data written. New bounds: 0x%08lX to 0x%08lX\n", DataStartAddr, DataEndAddr);
}


uint32_t W25Q_WriteVolume(uint32_t StartAddr, uint8_t *buff, uint32_t Len, bool CheckAddress) {
	uint32_t WriteAddr = StartAddr;
	if (CheckAddress) {
		WriteAddr = W25Q_GetSafeContiguousWriteAddress(WriteAddr);
	}

	uint32_t ToWrite = Len;
	uint32_t EndAddr = WriteAddr;

	// Write one page (256 bytes) at a time, checking for overflow into the next page
	while (ToWrite) {
		uint16_t PageRemain = 0x100 - (WriteAddr & 0xFF);
		uint16_t WriteLen = (ToWrite > PageRemain) ? PageRemain : ToWrite;

		W25Q_PageProgram(WriteAddr, buff + Len - ToWrite, WriteLen);

		ToWrite -= WriteLen;
		EndAddr = WriteAddr + WriteLen - 1;
		WriteAddr = EndAddr + 1;

		if (CheckAddress && ToWrite > 0) {   // Only safe address if we have more to write
			WriteAddr = W25Q_GetSafeContiguousWriteAddress(WriteAddr);
		}
	}

	printf("Volume written between 0x%08lX and 0x%08lX\n", StartAddr, EndAddr);

	return EndAddr;
}


bool W25Q_PageProgram(uint32_t StartAddr, uint8_t *buff, uint16_t Len) {
	uint16_t PageRemain = 0x100 - (StartAddr & 0xFF);   // Check length of write doesn't cross page boundary

	if (Len > PageRemain) {
		printf("ERROR IN W25Q_PageProgram: Write exceeds page length");
		return false;
	}

	W25Q_EnableWrite();

	uint8_t tx[261];   // Allocate enough space for a full page
	tx[0] = W25Q_PAGE_PROG_4BADDR;

	for (int i = 3; i >= 0; i--) {   // Load start address into tx buffer
		tx[4 - i] = (uint8_t)((StartAddr >> (i * 8)) & 0xFF);
	}

	for (int i = 0; i < Len; i++) {   // Load data into tx buffer
		tx[i + 5] = buff[i];
	}

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2_str, tx, Len + 5, W25Q_SPI_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	while (W25Q_CheckBusy()) {}

	return true;
}




void W25Q_EraseChip() {
	W25Q_EnableWrite();

	uint8_t tx[1] = {W25Q_CHIP_ERASE};

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2_str, tx, 1, W25Q_SPI_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	while (W25Q_CheckBusy()) {}

	printf("Erased chip\n");
}


void W25Q_EraseSector(uint32_t SectorAddr) {
	W25Q_EnableWrite();

	uint8_t tx[5] = {0};
	tx[0] = W25Q_SECTOR_ERASE_4BADDR;

	for (int i = 3; i >= 0; i--) {   // Load sector address into tx buffer
		tx[4 - i] = (uint8_t)((SectorAddr >> (i * 8)) & 0xFF);
	}

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2_str, tx, 5, W25Q_SPI_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	while (W25Q_CheckBusy()) {}

	printf("Erased sector 0x%08lX\n", SectorAddr);
}


void W25Q_EraseBlock(uint32_t BlockAddr) {
	W25Q_EnableWrite();

	uint8_t tx[5] = {0};
	tx[0] = W25Q_BLOCK_ERASE_4BADDR;

	for (int i = 3; i >= 0; i--) {   // Load sector address into tx buffer
		tx[4 - i] = (uint8_t)((BlockAddr >> (i * 8)) & 0xFF);
	}

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2_str, tx, 5, W25Q_SPI_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	while (W25Q_CheckBusy()) {}

	printf("Erased block 0x%08lX\n", BlockAddr);
}


void W25Q_EraseFlightData() {
	if (DataStartAddr == DataEndAddr) {
		printf("W25Q_EraseFlightData: No flight data to erase, returning early\n");
		return;
	}   // Return if no flight data to erase

	uint32_t StartSectorAddr = DataStartAddr & 0xFFFFF000;
	uint32_t EndSectorAddr = DataEndAddr & 0xFFFFF000;
	uint32_t CurrSectorAddr = StartSectorAddr;

	while (1) {
		W25Q_EraseSector(CurrSectorAddr);

		if (CurrSectorAddr == EndSectorAddr) { break; }   // End if the erased sector was the end sector

		CurrSectorAddr = W25Q_GetSafeContiguousReadAddress(CurrSectorAddr + 0x1000);   // Jump to the next safe sector

		// Failsafe in case the CurrSectorAddr wraps all the way around memory
		if (CurrSectorAddr == StartSectorAddr) {
			printf("ERROR: W25Q_EraseFlightData wrapped completely around memory");
			break;
		}
	}

	printf("Flight data erased between 0x%08lX and 0x%08lX\n", StartSectorAddr, EndSectorAddr);

	DataStartAddr = W25Q_GetSafeContiguousWriteAddress(EndSectorAddr + 0x1000);   // Move start position for new data to the next sector
	DataEndAddr = DataStartAddr;   // No data currently written
	W25Q_WriteMetadata(true);
}


void W25Q_ReadVolume(uint32_t StartAddr, uint8_t *buff, uint32_t MaxLen) {
	uint8_t tx[5];
	uint8_t rx[256];   // Allocate enough space for a full page

	tx[0] = W25Q_READ_4BADDR;

	uint32_t ReadLeft = MaxLen;
	uint32_t ReadAddr = StartAddr;
	uint32_t buffpos = 0;

	// Read one page at a time
	while (ReadLeft > 0) {
		uint16_t PageRemain = 0x100 - (ReadAddr & 0xFF);
		uint16_t ReadLen = 0;

		if (ReadLeft > PageRemain) {
			ReadLen = PageRemain;
		} else {
			ReadLen = ReadLeft;
		}

		for (int i = 3; i >= 0; i--) {   // Load start address into tx
			tx[4 - i] = (uint8_t)((ReadAddr >> (i * 8)) & 0xFF);
		}

		// Split the transmit and receive to save stack space for buffers
		HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
		HAL_SPI_Transmit(&hspi2_str, tx, 5, W25Q_SPI_MAX_DELAY);
		HAL_SPI_Receive(&hspi2_str, rx, ReadLen, W25Q_SPI_MAX_DELAY);
		HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

		// Put data in buffer and print out
		printf("Read volume: ");
		for (int i = 0; i < ReadLen; i++) {
			buff[i + buffpos] = rx[i];
			printf("%02X", rx[i]);
		}
		printf("\n");

		ReadAddr += ReadLen;
		buffpos += ReadLen;
		ReadLeft -= ReadLen;
	}
}


void W25Q_ReadVolumeSafe(uint32_t StartAddr, uint8_t *buff, uint32_t MaxLen) {
	uint8_t tx[5];
	uint8_t rx[256];   // Allocate enough space for a full page

	tx[0] = W25Q_READ_4BADDR;

	uint32_t ReadLeft = MaxLen;
	uint32_t ReadAddr = W25Q_GetSafeContiguousReadAddress(StartAddr);
	uint32_t buffpos = 0;

	// Read one page at a time
	while (ReadLeft > 0) {
		uint16_t PageRemain = 0x100 - (ReadAddr & 0xFF);
		uint16_t ReadLen = 0;

		if (ReadLeft > PageRemain) {
			ReadLen = PageRemain;
		} else {
			ReadLen = ReadLeft;
		}

		for (int i = 3; i >= 0; i--) {   // Load start address into tx
			tx[4 - i] = (uint8_t)((ReadAddr >> (i * 8)) & 0xFF);
		}

		// Split the transmit and receive to save stack space for buffers
		HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
		HAL_SPI_Transmit(&hspi2_str, tx, 5, W25Q_SPI_MAX_DELAY);
		HAL_SPI_Receive(&hspi2_str, rx, ReadLen, W25Q_SPI_MAX_DELAY);
		HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

		// Put data in buffer and print out
		printf("Read volume: ");
		for (int i = 0; i < ReadLen; i++) {
			buff[i + buffpos] = rx[i];

			printf("%02X", rx[i]);
		}
		printf("\n");

		ReadAddr = W25Q_GetSafeContiguousReadAddress(ReadAddr + ReadLen);
		buffpos += ReadLen;
		ReadLeft -= ReadLen;
	}
}



void ScanBadBlocks() {
	W25Q_EraseChip();   // Erase all existing data

	uint8_t CheckPattern[256];
	for (int i = 0; i < 256; i++) {
		CheckPattern[i] = (uint8_t)i;
	}

	uint8_t Readback[256];

	for (int block = 0; block < 512; block++) {
		bool bad = false;
		for (int page = 0; page < 256; page++) {
			uint32_t PageAddr = block * 0x10000 + page * 0x100;

			W25Q_PageProgram(PageAddr, CheckPattern, 256);
			W25Q_ReadVolume(PageAddr, Readback, 256);

			for (int i = 0; i < 256; i++) {
				if (Readback[i] != CheckPattern[i]) {   // If a byte doesn't match, the block is bad
					bad = true;
					break;
				}
			}

			if (bad) { break; }   // Skip the rest of the block if a page is bad
		}

		if (bad) {
			BadBlocks[block >> 3] |= (1U << (block & 0b111));   // Set bit in bad blocks array
			printf("Block %i identified as bad", block);
		}
	}

	W25Q_EraseChip();   // Perform final erase to reset chip
	W25Q_WriteMetadata(true);   // Rewrite metadata with new array
}

