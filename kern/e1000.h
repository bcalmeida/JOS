#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>

// Keys for the E1000 82540EM, the one used by QEMU
#define E1000_VENDOR_ID	0x8086
#define E1000_DEV_ID 	0x100E

// Registers
#define E1000_STATUS	0x00008  /* Device Status - RO */

// Functions headers
int attach_e1000(struct pci_func *pcif);

#endif	// JOS_KERN_E1000_H
