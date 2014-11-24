#include <kern/e1000.h>
#include <kern/pmap.h>

// Pointer to the start of E1000's MMIO region
volatile uint32_t *e1000;

// Initialize the E1000, which is a PCI device
// Returns 0 on success (always)
int
attach_e1000(struct pci_func *pcif)
{
	// Negotiates an MMIO region with the E1000 and stores
	// its base and size in BAR 0 (that is, reg_base[0] and reg_size[0])
	// Also fills irq_line with the interrupt line allocated for it
	pci_func_enable(pcif);

	// Map MMIO to an appropiate virtual memory
	physaddr_t pa = pcif->reg_base[0];
	size_t size = pcif->reg_size[0];
	e1000 = mmio_map_region(pa, size);

	// Test MMIO by accessing one of it's registers
	cprintf("E1000 MMIO test: Device Status Reg = %x (should be 80080783)\n",
		*(e1000 + 2)); // E1000_STATUS offset is 8
	return 0;
}
