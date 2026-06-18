#include "networking.h"


uint8_t ConstructNetPacket(uint8_t* buff, uint8_t maxlen, NetPacket* packetinfo) {
	// Check if the buffer is long enough to hold the packet
	if (maxlen < packetinfo->payloadlen + 5) { return 0; }

	buff[0] = packetinfo->recipient;
	buff[1] = packetinfo->sender;
	buff[2] = packetinfo->status;
	buff[3] = packetinfo->type;
	buff[4] = packetinfo->seqnum;

	for (int i = 0; i < packetinfo->payloadlen; i++) {
		buff[i + 5] = packetinfo->payload[i];
	}

	return packetinfo->payloadlen + 5;
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






