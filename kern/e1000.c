#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/string.h>

#include <kern/e1000.h>
#include <kern/pmap.h>

volatile uint32_t *e1000; // Pointer to the start of E1000's MMIO region
char *buffers[NUM_TX_DESC];
struct tx_desc *tx_ring;

/* Auxiliary Funcions */
static int e1000_page_alloc(void *va, int perm);
static physaddr_t va2pa(void *va);

/* Testing Functions */
static void check_mmio(int check);
static void check_mappings(int check);
static void show_mappings(int check);
static void test_transmission();

// Initializes transmision
void
init_transmission(void)
{
	// TODO: Throw the MMIOLIM logic to e1000_page_alloc
	cprintf("E1000 initializing transmission\n");

	/* Data structures setups */
	// Allocate memory for descriptor ring
	char *va = (char *) MMIOLIM;
	int r;
	if((r = e1000_page_alloc(va, PTE_P | PTE_W)) < 0)
		panic("e1000_page_alloc: %e", r);

	// Make descriptor ring point to that va
	tx_ring = (struct tx_desc *) va;

	// Allocate memory for the buffers (16)
	int i;
	for (i = 0; i < NUM_TX_DESC; i++) {
		va += PGSIZE;
		if((r = e1000_page_alloc(va, PTE_P | PTE_W)) < 0)
			panic("e1000_page_alloc: %e", r);
		buffers[i] = va;
	}

	// Test mappings
	check_mappings(1);
	show_mappings(1);

	// Initial settings of the tx_descriptors
	for (i = 0; i < NUM_TX_DESC; i++) {
		// Set CMD.RS, so it updates DD
		tx_ring[i].cmd |= E1000_TXD_CMD_RS;
		// Set STATUS.DD, so it starts available
		tx_ring[i].status |= E1000_TXD_STAT_DD;
	}

	/* Registers setup (as shown in chapter 14.5 of intel manual) */

	// TDBAH & TDBAL (Transmit Descriptor Ring address)
	// Always store physical address, not the virtual address!
	// E1000_REG(E1000_TDBAH) = 0;
	E1000_REG(E1000_TDBAL) = va2pa(tx_ring);

	// TDLEN (Transmit Descriptor Ring length in bytes)
	// Length is NUM_TX_DESC * sizeof(struct tx_desc), which is NUM_TX_DESC * 16
	E1000_REG(E1000_TDLEN) = NUM_TX_DESC * sizeof(struct tx_desc);

	// TDH & TDT (Transmit Decriptor Ring Head and Tail)
	E1000_REG(E1000_TDH) = 0;
	E1000_REG(E1000_TDT) = 0;

	// TCTL (Transmit Control Register)
	// Enable TCTL.EN
	E1000_REG(E1000_TCTL) |= E1000_TCTL_EN;
	// Enable TCTL.PSP
	E1000_REG(E1000_TCTL) |= E1000_TCTL_PSP;
	// Configure TCTL.CT
	/* No need */
	// Configure TCTL.COLD for full duplex operation (40h)
	E1000_REG(E1000_TCTL) &= (~E1000_TCTL_COLD | 0x00040000); // Hard coded...

	// TIPG
	E1000_REG(E1000_TIPG) = 0;
	E1000_REG(E1000_TIPG) += (10 <<  0); // TIPG.IPGT = 10, bits 0-9
	E1000_REG(E1000_TIPG) += ( 4 << 10); // TIPG.IPGR1 = 4 (2/3*IPGR2), 10-19
	E1000_REG(E1000_TIPG) += ( 6 << 20); // TIPG.IPGR2 = 6, bits 20-29
}

// Transmit packet
void
transmit_packet(void *buf, size_t size)
{
	// Initial checkings
	if (size > MAX_PACKET_SIZE)
		panic("Packet size is bigger than the maximum allowed");
	if (!buf)
		panic("Null pointer passed");

	// Retrieve tail and check if it is available (if ring is not full)
	// by checking if TXD.STATUS.DD is set
	uint32_t tail = E1000_REG(E1000_TDT);
	if (!(tx_ring[tail].status & 0x01)) {
		// Drop packet if tx_ring is full
		cprintf("tx_ring[tail] DD is not set: tx_ring is full. "
			"Transmission aborted.\n");
		return;
	}

	// Set tx_desc registers
	// Set CMD.EOP, meaning this is the end of packet (Really needed?)
	tx_ring[tail].cmd |= E1000_TXD_CMD_EOP;
	// Set STAT.DD to 0, meaning this tx_desc is not done, and needs to be sent
	tx_ring[tail].status &= ~E1000_TXD_STAT_DD;

	// Put packet data in buffer
	memset(buffers[tail], 0, PGSIZE);
	memmove(buffers[tail], buf, size);

	// Update tx descriptor
	tx_ring[tail].addr = (uint64_t) va2pa(buffers[tail]);
	tx_ring[tail].length = (uint16_t) size;

	// Debugging
	//cprintf("Transmitting packet from buf=%p, with size=%d\n", buf, size);
	//cprintf("tx_ring[tail].addr = %p\n", tx_ring[tail].addr);
	//cprintf("tx_ring[tail].length = %d\n", tx_ring[tail].length);

	// Update tail
	tail = (tail + 1) % NUM_TX_DESC;
	E1000_REG(E1000_TDT) = tail;
}


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

	check_mmio(1);

	init_transmission();
	test_transmission();
	return 0;
}

/* --------------------------- */
/* --- Auxiliary Functions --- */
/* --------------------------- */

// Custom page allocator for the E1000
// Returns:
//   0 on success
//   -E_INVAL if va is in user space
//   -E_NO_MEM if there is no more page to allocate a page or page table for mapping
static int
e1000_page_alloc(void *va, int perm)
{
	// Check if va is the kernel address space, so it is copied
	// to every environment
	if ((uint32_t) va < UTOP)
		return -E_INVAL;

	// Tries to allocate a physical page
	struct PageInfo *pp = page_alloc(ALLOC_ZERO);
	if (!pp) {
	        return -E_NO_MEM;
	}

	// Tries to map the physical page at va of kern_pgdir
	int r = page_insert(kern_pgdir, pp, va, perm);
	if (r < 0) {
	        page_free(pp);
	        return -E_NO_MEM;
	}
	return 0;
}

// Translates virtual address to physical address
// Could also use UVPT, but I think this way is simpler/easier
static physaddr_t
va2pa(void *va)
{
	struct PageInfo *pp = page_lookup(kern_pgdir, va, NULL);
	if (!pp)
		panic("va2pa: va is not mapped");
	return page2pa(pp);
}

/* ------------------------- */
/* --- Testing Functions --- */
/* ------------------------- */
static void
check_mmio(int check)
{
	if (!check) return;
	// Test MMIO by accessing one of it's status register
	assert(E1000_REG(E1000_STATUS) == 0x80080783);

	cprintf("E1000 MMIO is ok\n");
}

static void
check_mappings(int check)
{
	if (!check) return;

	// Check that the tx_ring page is present, writable and filled with zeroes
	char *va = (char *) tx_ring;
	*va = 'a';
	assert(*(va)      ==  'a');
	assert(*(va+10)   == '\0');
	assert(*(va+100)  == '\0');
	assert(*(va+1000) == '\0');
	*va = 0;

	// Check that buffers pages are all present, writable and filled with
	// zeroes, by comparing it to the tx_ring page
	int i;
	for (i = 0; i < NUM_TX_DESC; i++) {
		assert(memcmp(tx_ring, buffers[i], PGSIZE) == 0);
	}

	cprintf("E1000 TX mappings are ok\n");
}

static void
show_mappings(int check)
{
	if (!check) return;

	// Print all the TX mappings
	cprintf("E1000 TX mappings:\n");
	cprintf("\ttx_ring       \tva=%p, \tpa=%p\n", tx_ring, va2pa(tx_ring));
	int i;
	for (i = 0; i < NUM_TX_DESC; i++) {
		cprintf("\tbuffers[%d] \tva=%p, \tpa=%p\n",
			i, buffers[i], va2pa(buffers[i]));
	}
}

static void
test_transmission(void)
{
	uint32_t data[50];
	int i;

	for (i = 0; i < 50; i++) {
		data[i] = i;
	}

	for (i = 0; i < 18; i++) {
		transmit_packet(&data, sizeof(data));
	}
}
