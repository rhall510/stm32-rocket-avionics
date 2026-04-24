#include "w25q.h"
#include "stm32g4xx.h"


extern SPI_HandleTypeDef hspi2_str;





bool InitialiseW25Q() {
	W25Q_Reset();

	// Check device ID
	uint8_t tx[4] = {0};
	uint8_t rx[4] = {0};

	tx[0] = W25Q_JEDECID;

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(&hspi2_str, tx, rx, 4, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	if (rx[1] != 0xEF || (((uint16_t)rx[2] << 8) + rx[3]) != 0x4019) {
		return false;
	}

	// Set to 4 byte address mode
	tx[0] = W25Q_4BYTE_ADDR_MODE;

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2_str, tx, 1, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	return true;
}


void W25Q_Reset() {
	// Enable software reset
	uint8_t tx[1] = {W25Q_ENABLE_RESET};

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2_str, tx, 1, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	// Execute software reset
	tx[0] = W25Q_RESET;

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2_str, tx, 1, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	HAL_Delay(1);   // Must wait at least 30uS
}


bool W25Q_CheckBusy() {
	// Read status 1 register
	uint8_t tx[2] = {W25Q_READ_STATUS1, 0};
	uint8_t rx[2] = {0};

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(&hspi2_str, tx, rx, 2, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	return rx[1] & 1;
}


void W25Q_EnableWrite() {
	uint8_t tx[1] = {W25Q_WRITE_ENABLE};

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2_str, tx, 1, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);
}




bool W25Q_ReadMetadata() {
	MetaStartAddr = 1;   // 1 Indicates not found

	uint8_t tx[73] = {0};   // Allocate enough space for reading full metadata later
	uint8_t rx[73] = {0};

	tx[0] = W25Q_READ_4BADDR;

	// Read 8 bytes at the start of each block and check for metadata start identifier
	for (int block = 0; block < 512; block++) {
		uint32_t ReadAddr = (block << 16);
		for (int i = 3; i >= 0; i--) {   // Load block start address into tx
			tx[4 - i] = (uint8_t)(ReadAddr & (0xFF >> (i * 8)));
		}

		HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
		HAL_SPI_TransmitReceive(&hspi2_str, tx, rx, 13, HAL_MAX_DELAY);
		HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

		bool found = true;
		for (int i = 0; i < 8; i++) {   // Check for metadata start ID
			if (rx[i + 5] != (uint8_t)(W25Q_META_START_ID >> ((7 - i) * 8))) {
				found = false;
				break;
			}
		}

		if (found) {
			MetaStartAddr = ReadAddr;
			break;
		}
	}

	// Create new metadata block if none found. Must assume no bad blocks and reset addresses to start
	if (MetaStartAddr == 1) {
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
		tx[4 - i] = (uint8_t)(ReadAddr & (0xFF >> (i * 8)));
	}

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(&hspi2_str, tx, rx, 73, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	DataStartAddr = ((uint32_t)rx[5] << 24) | ((uint32_t)rx[6] << 16) | ((uint32_t)rx[7] << 8) | rx[8];

	for (int i = 0; i < 64; i++) {   // Store bad block array
		BadBlocks[i] = rx[i + 9];
	}

	MetaEndAddr = MetaStartAddr + 0xFFF;   // Reserve the entire sector for metadata


	// Find data end address
	ReadAddr = DataStartAddr;
	while (1) {
		// Read 8 bytes at packet start: 4 byte sync word + 4 byte header
		for (int i = 3; i >= 0; i--) {
			tx[4 - i] = (uint8_t)(ReadAddr & (0xFF >> (i * 8)));
		}

		HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
		HAL_SPI_TransmitReceive(&hspi2_str, tx, rx, 13, HAL_MAX_DELAY);
		HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

		uint32_t sync = ((uint32_t)rx[5] << 24) | ((uint32_t)rx[6] << 16) | ((uint32_t)rx[7] << 8) | rx[8];
		if (sync == W25Q_DATA_SYNC_WORD) {   // Increment read address to expected start of the next packet
			ReadAddr = W25Q_GetSafeContiguousReadAddressWithOffset(ReadAddr, (((uint16_t)rx[10] << 8) | rx[11]));
		} else {
			DataEndAddr = (ReadAddr == DataStartAddr) ? ReadAddr : ReadAddr - 1;   // Make start and end addresses equal if no data
			break;
		}
	}

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
		metadata[i] = (uint8_t)(W25Q_META_START_ID >> (i * 8));
	}

	for (int i = 0; i < 4; i++) {   // Data start address
		metadata[i + 8] = (uint8_t)(DataStartAddr >> (i * 8));
	}

	for (int i = 0; i < 64; i++) {   // Bad block array
		metadata[i + 12] = BadBlocks[i];
	}

	W25Q_WriteVolume(MetaStartAddr, metadata, len, false);   // Write without address checking
}



uint32_t W25Q_GetSafeContiguousWriteAddress(uint32_t Addr) {
	uint32_t SafeAddr = Addr;

	// Do all checks and recheck if the address moves
	while (1) {
		// Check if the address extends beyond the end of memory
		if (SafeAddr > 0x01FFFFFF) {
			printf("WARNING: Address wraparound in W25Q_GetSafeContiguousWriteAddress");
			SafeAddr = 0x0;
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
		} else {
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
		if (SafeAddr > 0x01FFFFFF) {
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

	if (Offset > PageLeft) {
		OffsetLeft -= PageLeft;
		SafeAddr += PageLeft;
	} else {
		SafeAddr += OffsetLeft;
		OffsetLeft = 0;
	}

	while (OffsetLeft > 0) {
		SafeAddr = W25Q_GetSafeContiguousReadAddress(SafeAddr);

		if (Offset > 256) {
			OffsetLeft -= 256;
			SafeAddr += 256;
		} else {
			SafeAddr += OffsetLeft;
			OffsetLeft = 0;
		}
	}

	return SafeAddr;
}


uint32_t W25Q_GetSafeMetadataAddress() {
	uint32_t SafeAddr = 0x0;
	bool safe = false;

	// Check each block start address
	for (int block = 0; block < 512; block++) {
		SafeAddr = (block << 16);

		// Check if the address is in the data section
		if (DataEndAddr > DataStartAddr) {
			if (SafeAddr <= DataEndAddr && SafeAddr >= DataStartAddr) { continue; }
		} else {
			if (SafeAddr <= DataEndAddr || SafeAddr >= DataStartAddr) { continue; }
		}

		// Check if the address is in a bad block
		uint16_t BlockNum = SafeAddr >> 16;
		if (BadBlocks[BlockNum >> 3] & (1U << (BlockNum & 0b111))) { continue; }

		break;   // Break if all tests pass
	}

	return safe ? SafeAddr : 1;
}




void W25Q_WriteAppendData(uint8_t *buff, uint32_t Len) {
	DataEndAddr = W25Q_WriteVolume(DataEndAddr + 1, buff, Len, true);
}


uint32_t W25Q_WriteVolume(uint32_t StartAddr, uint8_t *buff, uint32_t Len, bool CheckAddress) {
	uint32_t WriteAddr = StartAddr;
	if (CheckAddress) {
		WriteAddr = W25Q_GetSafeContiguousWriteAddress(WriteAddr);
	}

	uint32_t ToWrite = Len;
	uint32_t EndAddr = 0;

	// Write one page (256 bytes) at a time, checking for overflow into the next page
	while (ToWrite) {
		uint16_t PageRemain = 0x100 - (WriteAddr & 0xFF);
		uint16_t WriteLen = 0;

		if (ToWrite > PageRemain) {
			WriteLen = PageRemain;
		} else {
			WriteLen = ToWrite;
		}

		W25Q_PageProgram(WriteAddr, buff, WriteLen);

		ToWrite -= WriteLen;
		EndAddr = WriteAddr + WriteLen;
		WriteAddr = WriteAddr + WriteLen;

		if (CheckAddress) {
			WriteAddr = W25Q_GetSafeContiguousWriteAddress(WriteAddr);
		}
	}

	return EndAddr;
}


bool W25Q_PageProgram(uint32_t StartAddr, uint8_t *buff, uint16_t Len) {
	uint16_t PageRemain = 0x100 - (StartAddr & 0xFF);   // Check length of write doesn't cross page boundary

	if (Len > PageRemain) {
		printf("ERROR IN W25Q_PageProgram: Write exceeds page length");
		return false;
	}

	W25Q_EnableWrite();

	uint8_t tx[Len + 5];
	tx[0] = W25Q_PAGE_PROG_4BADDR;

	for (int i = 3; i >= 0; i--) {   // Load start address into tx buffer
		tx[4 - i] = (uint8_t)(StartAddr & (0xFF >> (i * 8)));
	}

	for (int i = 0; i < Len; i++) {   // Load data into tx buffer
		tx[i + 5] = buff[i];
	}

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2_str, tx, Len + 5, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	while (W25Q_CheckBusy()) {}

	return true;
}




void W25Q_EraseChip() {
	W25Q_EnableWrite();

	uint8_t tx[1] = {W25Q_CHIP_ERASE};

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2_str, tx, 1, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	while (W25Q_CheckBusy()) {}
}


void W25Q_EraseSector(uint32_t SectorAddr) {
	W25Q_EnableWrite();

	uint8_t tx[5] = {0};
	tx[0] = W25Q_SECTOR_ERASE_4BADDR;

	for (int i = 3; i >= 0; i--) {   // Load sector address into tx buffer
		tx[4 - i] = (uint8_t)(SectorAddr & (0xFF >> (i * 8)));
	}

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2_str, tx, 1, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	while (W25Q_CheckBusy()) {}
}


void W25Q_EraseBlock(uint32_t BlockAddr) {
	W25Q_EnableWrite();

	uint8_t tx[5] = {0};
	tx[0] = W25Q_BLOCK_ERASE_4BADDR;

	for (int i = 3; i >= 0; i--) {   // Load sector address into tx buffer
		tx[4 - i] = (uint8_t)(BlockAddr & (0xFF >> (i * 8)));
	}

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2_str, tx, 1, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	while (W25Q_CheckBusy()) {}
}




void W25Q_ReadVolume(uint32_t StartAddr, uint8_t *buff, uint32_t MaxLen) {
	uint8_t tx[261];   // Allocate enough space for a full page
	uint8_t rx[261];

	tx[0] = W25Q_READ_4BADDR;

	uint32_t ReadLeft = MaxLen;
	uint32_t ReadAddr = StartAddr;
	uint32_t buffpos = 0;

	// Read one page at a time
	while (ReadLeft > 0) {
		uint16_t PageRemain = 0x100 - (WriteAddr & 0xFF);
		uint16_t ReadLen = 0;

		if (ReadLeft > PageRemain) {
			ReadLen = PageRemain;
		} else {
			ReadLen = ReadLeft;
		}

		for (int i = 3; i >= 0; i--) {   // Load start address into tx
			tx[4 - i] = (uint8_t)(ReadAddr & (0xFF >> (i * 8)));
		}

		HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
		HAL_SPI_TransmitReceive(&hspi2_str, tx, rx, ReadLen + 5, HAL_MAX_DELAY);
		HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

		// Put data in buffer and print out
		for (int i = 5; i < 261; i++) {
			buff[i - 5 + buffpos] = rx[i];
			printf("%x", rx[i]);
		}
		printf("\n");

		ReadAddr += ReadLen;
		buffpos += ReadLen;
	}
}


void W25Q_ReadVolumeSafe(uint32_t StartAddr, uint8_t *buff, uint32_t MaxLen) {
	uint8_t tx[261];   // Allocate enough space for a full page
	uint8_t rx[261];

	tx[0] = W25Q_READ_4BADDR;

	uint32_t ReadLeft = MaxLen;
	uint32_t ReadAddr = W25Q_GetSafeContiguousReadAddress(StartAddr);
	uint32_t buffpos = 0;

	// Read one page at a time
	while (ReadLeft > 0) {
		uint16_t PageRemain = 0x100 - (WriteAddr & 0xFF);
		uint16_t ReadLen = 0;

		if (ReadLeft > PageRemain) {
			ReadLen = PageRemain;
		} else {
			ReadLen = ReadLeft;
		}

		for (int i = 3; i >= 0; i--) {   // Load start address into tx
			tx[4 - i] = (uint8_t)(ReadAddr & (0xFF >> (i * 8)));
		}

		HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
		HAL_SPI_TransmitReceive(&hspi2_str, tx, rx, ReadLen + 5, HAL_MAX_DELAY);
		HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

		// Put data in buffer and print out
		for (int i = 5; i < 261; i++) {
			buff[i - 5 + buffpos] = rx[i];
			printf("%x", rx[i]);
		}
		printf("\n");

		ReadAddr = W25Q_GetSafeContiguousReadAddress(ReadAddr + ReadLen);
		buffpos += ReadLen;
	}
}



void ScanBadBlocks() {
	W25Q_EraseChip();   // Erase all existing data

	uint8_t CheckPattern[256];
	for (int i = 0; i < 256; i++) {
		CheckPattern[i] = i & 0xF;
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
			BadBlocks[block >> 3] |= (1U << (BlockNum & 0b111));   // Set bit in bad blocks array
			printf("Block %i identified as bad", block);
		}
	}

	W25Q_EraseChip();   // Perform final erase to reset chip
	W25Q_WriteMetadata(true);   // Rewrite metadata with new array
}

