#ifndef NETWORKING_H_
#define NETWORKING_H_

#include <stdint.h>


// Node addresses
#define NET_BROADCAST_ADDR 0xFFU
#define NET_CONTROLLER_ADDR 0x0U
#define NET_AVIONICS_ADDR 0x1U
#define NET_RNODE1_ADDR 0x2U
#define NET_RNODE2_ADDR 0x3U


// Message type codes
typedef enum {
	NET_MTYPE_ACK,
	NET_MTYPE_DISCOVERY,
	NET_MTYPE_PKTTEST,
	NET_MTYPE_TRSMT_DATA,
	NET_MTYPE_TRSMT_DATA_TERM,
	NET_MTYPE_TRSMT_DATA_RETR
} NetMessageType;


// Packets
#define NET_PACKET_MAXLEN 256
#define NET_HEADER_LEN 6
#define NET_PAYLOAD_MAXLEN 250

typedef struct {
	uint8_t recipient;
	uint8_t sender;
	uint8_t status;
	uint8_t type;
	uint8_t seqnum;
	uint8_t payloadlen;
	uint8_t payload[NET_PAYLOAD_MAXLEN];
} NetPacket;


// Helper functions
// Construct a standard network packet in the provided buffer with the provided data. Returns the total length of the constructed packet
uint8_t ConstructNetPacket(uint8_t* buff, uint8_t maxlen, NetPacket* packetinfo);

// Decode information from raw network packet bytes and place into a NetPacket struct
void DecodeNetPacket(NetPacket* packetinfo, uint8_t* buff, uint8_t bufflen);

#endif /* NETWORKING_H_ */
