#ifndef USBCMD_H_
#define USBCMD_H_

#include <stdint.h>
#include <stdbool.h>


// Message/command type codes
typedef enum {
	USB_MTYPE_STATUS,
	USB_MTYPE_ECHO,
	USB_MTYPE_DISCOVERY
} USBMessageType;


// Packets
#define USB_PAYLOAD_MAXLEN 250
#define USB_SYNC_WORD 0xBEEBU

typedef struct {
	USBMessageType type;
	uint8_t payloadlen;
	uint8_t payload[USB_PAYLOAD_MAXLEN];
} USBPacket;


// Helper functions
// Construct a standard USB packet in the provided buffer with the provided data. Returns the total length of the constructed packet
uint16_t ConstructUSBPacket(uint8_t* buff, uint16_t maxlen, USBPacket* packetinfo);

#endif /* USBCMD_H_ */
