#include "w25q.h"
#include "stm32g4xx.h"


extern SPI_HandleTypeDef hspi2_str;


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




bool InitialiseW25Q(bool Erase) {
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



bool W25Q_ReadMetadata(uint32_t *DataAddr, uint16_t *NumBB, uint16_t *BB) {
	MetaStartAddr = 1;   // 1 Indicates not found

	uint8_t tx[13] = {0};
	uint8_t rx[13] = {0};

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
		DataStartAddr = 0x1000;   // No point in performing safety checks without bad block data
		NumBadBlocks = 0;

		W25Q_WriteMetadata(0x0, 0x1000, 0, BadBlocks);
		return false;
	}

	// Read existing metadata if found
	// Start with the first 6 bytes encoding the data start address (4 bytes) and metadata length (2 bytes)
	uint32_t ReadAddr = MetaStartAddr + 8;
	for (int i = 3; i >= 0; i--) {
		tx[4 - i] = (uint8_t)(ReadAddr & (0xFF >> (i * 8)));
	}

	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(&hspi2_str, tx, rx, 11, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

	DataStartAddr = ((uint32_t)rx[5] << 24) | ((uint32_t)rx[6] << 16) | ((uint32_t)rx[7] << 8) | rx[8];
	uint16_t bbarrlen = ((uint16_t)rx[9] << 8) | rx[10];

	NumBadBlocks = bbarrlen >> 1;   // 2 bytes for each bad block address

	if (NumBadBlocks) {
		// Read bad block array if not empty
		uint8_t tx2[bbarrlen] = {0};
		uint8_t RawBlocks[bbarrlen];

		tx2[0] = W25Q_READ_4BADDR;

		ReadAddr += 6;
		for (int i = 3; i >= 0; i--) {
			tx[4 - i] = (uint8_t)(ReadAddr & (0xFF >> (i * 8)));
		}

		HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET);
		HAL_SPI_TransmitReceive(&hspi2_str, tx2, RawBlocks, bbarrlen, HAL_MAX_DELAY);
		HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET);

		for (int block = 0; block < NumBadBlocks; block++) {
			BadBlocks[block] = ((uint16_t)RawBlocks[block * 2 + 5] << 8) | RawBlocks[block * 2 + 6];
		}
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
			ReadAddr = ReadAddr + ((uint16_t)rx[10] << 8) | rx[11];
		} else {
			DataEndAddr = ReadAddr - 1;
			break;
		}
	}

	return true;
}




void W25Q_WriteMetadata(uint32_t *DataAddr, uint16_t *NumBB, uint16_t *BB, bool ErasePrev) {
	if (ErasePrev) {
		W25Q_EraseSector(MetaStartAddr);
	}

	MetaStartAddr = W25Q_GetSafeMetadataAddress();   // Get first valid metadata address
	MetaEndAddr = MetaStartAddr + 0xFFF;      // Reserve the entire sector for metadata

	if (MetaStartAddr == 1) {
		printf("ERROR in W25Q_WriteMetadata: Cannot find valid metadata sector address");
		return;
	}

	uint16_t len = 8 + 4 + 2 + 2 * NumBB;
	uint8_t metadata[len] = {0};

	for (int i = 0; i < 8; i++) {   // Start identifier
		metadata[i] = (uint8_t)(W25Q_META_START_ID >> (i * 8));
	}

	for (int i = 0; i < 4; i++) {   // Data start address
		metadata[i + 8] = (uint8_t)(DataAddr >> (i * 8));
	}

	// Bad block array length
	metadata[12] = (uint8_t)(NumBB >> 8);
	metadata[13] = (uint8_t)NumBB;

	for (int i = 0; i < NumBB; i++) {   // Bad block array
		metadata[i + 14] = (uint8_t)(BB[i] >> 8);
		metadata[i + 15] = (uint8_t)BB[i];
	}

	W25Q_WriteVolume(DataEndAddress + 1, metadata, len);
}


uint32_t W25Q_GetSafeContiguousAddress(uint32_t Addr) {
	uint32_t SafeAddr = Addr;

	// Do all checks and recheck if the address moves
	while (1) {
		// Check if the address extends beyond the end of memory
		if (SafeAddr > 0x01FFFFFF) {
			printf("WARNING: Address wraparound in W25Q_GetSafeContiguousAddress");
			SafeAddr = Addr - 0x02000000;
			continue;
		}

		// Check if the address is in the metadata sector
		if (SafeAddr <= MetaEndAddr && SafeAddr >= MetaStartAddr) {
			printf("WARNING: Address moved out of metadata sector in W25Q_GetSafeContiguousAddress");
			SafeAddr = MetaEndAddr + 1;
			continue;
		}

		// Check if the address is in the data section
		if (SafeAddr <= DataEndAddr && SafeAddr >= DataStartAddr) {
			printf("WARNING: Address moved out of data section in W25Q_GetSafeContiguousAddress");
			SafeAddr = DataEndAddr + 1;
			continue;
		}

		// Check if the address is in a bad block
		for (int i = 0; i < NumBadBlocks; i++) {
			uint32_t BBStartAddr = (uint32_t)BadBlocks[i] * 0x10000;
			uint32_t BBEndAddr = BBStartAddr + 0xFFFF;

			if (SafeAddr <= BBEndAddr && SafeAddr >= BBStartAddr) {
				SafeAddr = BBEndAddr + 1;
				printf("WARNING: Address moved out of bad block in W25Q_GetSafeContiguousAddress");
				continue;
			}
		}

		break;   // Break if all tests pass
	}

	return SafeAddr;
}


uint32_t W25Q_GetSafeMetadataAddress() {
	uint32_t SafeAddr = Addr;
	bool safe = false;

	// Check each block start address
	for (int block = 0; block < 512; block++) {
		SafeAddr = (block << 16);

		// Check if the address is in the data section
		if (SafeAddr <= DataEndAddr && SafeAddr >= DataStartAddr) {
			continue;
		}

		// Check if the address is in a bad block
		for (int i = 0; i < NumBadBlocks; i++) {
			if (BadBlocks[i] == block) {
				continue;
			}
		}

		break;   // Break if all tests pass
	}

	if (safe) {
		return SafeAddr;
	} else {
		return 1;
	}
}



void W25Q_WriteAppendData(uint8_t *buff, uint32_t Len) {
	DataEndAddress = W25Q_WriteVolume(DataEndAddress + 1, buff, Len);
}


uint32_t W25Q_WriteVolume(uint32_t StartAddr, uint8_t *buff, uint32_t Len) {
	uint32_t WriteAddr = W25Q_GetSafeContiguousAddress(StartAddr);
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
		WriteAddr = W25Q_GetSafeContiguousAddress(WriteAddr + WriteLen);
	}

	return EndAddr;
}


bool W25Q_PageProgram(uint32_t StartAddr, uint8_t *buff, uint16_t Len) {
	uint16_t PageRemain = 0x100 - (StartAddr & 0xFF);   // Check length of write doesn't cross page boundary

	if (Len > PageRemain) {
		printf("ERROR IN W25Q_PageProgram: Write exceeds page length");
		return false;
	}

	uint8_t tx[Len + 5] = {0};
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

	return true;
}












