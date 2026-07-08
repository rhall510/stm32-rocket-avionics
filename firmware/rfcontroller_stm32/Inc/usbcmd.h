#ifndef USBCMD_H_
#define USBCMD_H_

#include <stdint.h>
#include <stdbool.h>


// Message/command type codes with definitions
typedef enum {
	USB_MTYPE_STATUS,
	/* Return the current status of the device. No payload. !! CURRENTLY NOT IMPLEMENTED !!
	 */
	USB_MTYPE_INFO,
	/* General info sent by the device to the host. Variable payload.
	 */
	USB_MTYPE_ECHO,
	/* Echo back the packet sent to the device. Payload is the same as sent by the host.
	 */
	USB_MTYPE_DISCOVERY,
	/* Notification of a discovery response sent from device > host. 1 byte payload of the node address that responded.
	 */
	USB_MTYPE_PKTTEST,
	/* Command the device to start transmitting packet test requests to the specified node address. 1 byte payload of the target node address.
	 */
	USB_MTYPE_STOP,
	/* Command the device to stop the current action. No payload.
	 */
	USB_MTYPE_DATA_DOWNLOAD
	/* Command the device to request a range of data from the avionics unit. Also used to relay the received data.
	 * HOST PAYLOAD: 8 bytes - [0:3] Number of bytes to transmit (0 means all available), [4:7] Offset in bytes from data start address to start reading
	 * DEVICE PAYLOAD: Variable - [0:3] Sequence number, [4:] Requested data from each received packet
	 */
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
