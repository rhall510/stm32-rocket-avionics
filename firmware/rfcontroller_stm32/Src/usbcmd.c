#include "usbcmd.h"


uint16_t ConstructUSBPacket(uint8_t* buff, uint16_t maxlen, USBPacket* packetinfo) {
	// Check if the buffer is long enough to hold the packet
	if (maxlen < packetinfo->payloadlen + 4) { return 0; }

	buff[0] = USB_SYNC_WORD >> 8;
	buff[1] = USB_SYNC_WORD & 0xFF;
	buff[2] = (uint8_t)packetinfo->type;
	buff[3] = packetinfo->payloadlen;

	for (int i = 0; i < packetinfo->payloadlen; i++) {
		buff[i + 4] = packetinfo->payload[i];
	}

	return packetinfo->payloadlen + 4;
}









