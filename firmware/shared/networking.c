#include "networking.h"


uint8_t ConstructNetPacket(uint8_t* buff, uint8_t maxlen, NetPacket* packetinfo) {
	// Check if the buffer is long enough to hold the packet
	if (maxlen < packetinfo->payloadlen + 6) { return 0; }

	buff[0] = packetinfo->payloadlen + 5;   // Start with length EXCLUDING itself in variable length mode
	buff[1] = packetinfo->recipient;
	buff[2] = packetinfo->sender;
	buff[3] = packetinfo->status;
	buff[4] = packetinfo->type;
	buff[5] = packetinfo->seqnum;

	for (int i = 0; i < packetinfo->payloadlen; i++) {
		buff[i + 6] = packetinfo->payload[i];
	}

	return packetinfo->payloadlen + 6;
}

void DecodeNetPacket(NetPacket* packetinfo, uint8_t* buff, uint8_t bufflen) {
	packetinfo->recipient = buff[0];
	packetinfo->sender = buff[1];
	packetinfo->status = buff[2];
	packetinfo->type = buff[3];
	packetinfo->seqnum = buff[4];
	packetinfo->payloadlen = bufflen - 5;

	for (int i = 0; i < packetinfo->payloadlen; i++) {
		packetinfo->payload[i] = buff[i + 5];
	}
}






