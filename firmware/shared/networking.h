#ifndef NETWORKING_H_
#define NETWORKING_H_

#include <stdint.h>


// Node addresses
#define NET_BROADCAST_ADDR 0xFFU
#define NET_CONTROLLER_ADDR 0x0U
#define NET_AVIONICS_ADDR 0x1U
#define NET_RNODE1_ADDR 0x2U
#define NET_RNODE2_ADDR 0x3U


// Message type codes with definitions
typedef enum {
	NET_MTYPE_ACK,
	/* Simple acknowledgement of the latest received command. Variable payload depending on what it is responding to.
	 */
	NET_MTYPE_NACK,
	/* Simple negative acknowledgement of the latest received command. Variable payload depending on what it is responding to.
	 */
	NET_MTYPE_DISCOVERY,
	/* Request to receive an ACK delayed by 50*ADDR ms. No payload.
	 */
	NET_MTYPE_PKTTEST,
	NET_MTYPE_GET_DATA_RANGE,
	/* Request to receive the quantity of flight data stored in flash.
	 * SENDER PAYLOAD: empty
	 * RESPONDER PAYLOAD: 8 bytes - [0:3] Total bytes, [4:7] Total data packets
	 */
	NET_MTYPE_TRSMT_DATA,
	/* Request to transmit flight data stored in flash. Can optionally specify a range of sequence numbers to transmit if known.
	 * SENDER PAYLOAD: 8 bytes - [0:3] Number of bytes to transmit, [4:7] Offset in bytes from data start address to start reading
	 * RESPONDER PAYLOAD: Variable - [0:3] Sequence number, [4:] Requested data split into multiple packets if needed
	 */
} NetMessageType;



// Packets
#define NET_PACKET_MAXLEN 255
#define NET_HEADER_LEN 6
#define NET_PAYLOAD_MAXLEN 249

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
