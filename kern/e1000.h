#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>

// Constants
#define MAX_PACKET_SIZE 1518
#define NUM_TX_DESC 16  // Multiple of 8, at maximum 64
#define NUM_RX_DESC 128 // Multiple of 8, at least 128

// Mac Address 52:54:00:12:34:56 (Attention: reversed byte order)
#define MAC_ADDR_LOW_32  0x12005452  /* 52:54:00:12 */
#define MAC_ADDR_HIGH_16 0x5634      /* 34:56 */

// Macro to access the registers in MMIO
#define E1000_REG(offset) *(e1000 + (offset>>2))

// Keys for the E1000 82540EM, the one used by QEMU
#define E1000_VENDOR_ID 0x8086
#define E1000_DEV_ID    0x100E

// Register used to test MMIO
#define E1000_STATUS   0x00008  /* Device Status - RO */

/* -- Registers used in TX -- */
// Other tx registers
#define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */

// Tx ring registers
#define E1000_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    0x03804  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN    0x03808  /* TX Descriptor Length - RW */
#define E1000_TDH      0x03810  /* TX Descriptor Head - RW */
#define E1000_TDT      0x03818  /* TX Descripotr Tail - RW */

// Transmit Control Register
#define E1000_TCTL     0x00400  /* TX Control - RW */
#define E1000_TCTL_RST    0x00000001    /* software reset */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_BCE    0x00000004    /* busy check enable */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */
#define E1000_TCTL_SWXOFF 0x00400000    /* SW Xoff transmission */
#define E1000_TCTL_PBE    0x00800000    /* Packet Burst Enable */
#define E1000_TCTL_RTLC   0x01000000    /* Re-transmit on late collision */
#define E1000_TCTL_NRTU   0x02000000    /* No Re-transmit on underrun */
#define E1000_TCTL_MULR   0x10000000    /* Multiple request support */

// Transmit Descriptor bit definitions
#define E1000_TXD_CMD_EOP  0x01
#define E1000_TXD_CMD_RS   0x08
#define E1000_TXD_STAT_DD  0x01

/* -- Registers used in RX -- */
// Other rx registers
#define E1000_RAL0     0x05400      /* Receive Mac Address Low */
#define E1000_RAH0     0x05404      /* Receive Mac Address High */
#define E1000_RAH0_AV             0x80000000  /* RAH0's Adress Valid (AV) bit */
#define E1000_MTA      0x05200  /* Multicast Table Array - RW Array */
#define E1000_IMS      0x000D0  /* Interrupt Mask Set - RW */

// Rx ring registers
#define E1000_RDBAL    0x02800  /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    0x02804  /* RX Descriptor Base Address High - RW */
#define E1000_RDLEN    0x02808  /* RX Descriptor Length - RW */
#define E1000_RDH      0x02810  /* RX Descriptor Head - RW */
#define E1000_RDT      0x02818  /* RX Descriptor Tail - RW */

// Receive Control Register
#define E1000_RCTL     0x00100  /* RX Control - RW */
#define E1000_RCTL_EN             0x00000002    /* enable */
#define E1000_RCTL_BAM            0x00008000    /* broadcast enable */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */

// Receive Descriptor bit definitions
#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */
#define E1000_RXD_STAT_EOP      0x02    /* End of Packet */

/* Functions headers */
int attach_e1000(struct pci_func *pcif);
void transmit_packet(void *buf, size_t size);
void receive_packet(void *buf, size_t* size_store);

/* Structures */

// Transmit descriptor
// 63            48 47   40 39   32 31   24 23   16 15             0
// +---------------------------------------------------------------+
// |                         Buffer address                        |
// +---------------+-------+-------+-------+-------+---------------+
// |    Special    |  CSS  | Status|  Cmd  |  CSO  |    Length     |
// +---------------+-------+-------+-------+-------+---------------+
struct tx_desc
{
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
};

// Receive descriptor
// 63            48 47   40 39   32 31           16 15             0
// +---------------------------------------------------------------+
// |                         Buffer address                        |
// +---------------+-------+-------+---------------+---------------+
// |    Special    | Errors| Status|  Pkt Checksum |    Length     |
// +---------------+-------+-------+---------------+---------------+
struct rx_desc
{
	uint64_t addr;
	uint16_t length;
	uint16_t pkt_checksum;
	uint8_t status;
	uint8_t errors;
	uint16_t special;
};

#endif	// JOS_KERN_E1000_H
