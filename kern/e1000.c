#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/string.h>

#include <kern/e1000.h>
#include <kern/pmap.h>

volatile uint32_t *e1000; // Pointer to the start of E1000's MMIO region
uint8_t mac_address[6]; // Mac address, 6 bytes
struct tx_desc *tx_ring;
char *tx_buffers[NUM_TX_DESC];
struct rx_desc *rx_ring;
char *rx_buffers[NUM_RX_DESC];

/* Auxiliary Funcions */
static int e1000_page_alloc(char **va_store, int perm);
static physaddr_t va2pa(void *va);

/* Testing Functions */
static void check_mmio(int check);
static void check_tx_mappings(int check);
static void show_tx_mappings(int check);
static void check_rx_mappings(int check);
static void show_rx_mappings(int check);
static void test_transmission();
static void test_receive();

// Initializes transmision
void
init_transmission(void)
{
	cprintf("E1000 initializing transmission\n");

	/* Data structures setup */
	// Allocate memory for descriptor ring
	char *va;
	int r;
	if((r = e1000_page_alloc(&va, PTE_P | PTE_W)) < 0)
		panic("e1000_page_alloc: %e", r);
	tx_ring = (struct tx_desc *) va;

	// Allocate memory for the buffers
	int i;
	for (i = 0; i < NUM_TX_DESC; i++) {
		if((r = e1000_page_alloc(&va, PTE_P | PTE_W)) < 0)
			panic("e1000_page_alloc: %e", r);
		tx_buffers[i] = va;
	}

	// Test mappings
	check_tx_mappings(1);
	show_tx_mappings(1);

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
	// Set CMD.EOP, meaning this is the end of packet (TODO: Really needed?)
	tx_ring[tail].cmd |= E1000_TXD_CMD_EOP;
	// Set STAT.DD to 0, meaning this tx_desc is not done, and needs to be sent
	tx_ring[tail].status &= ~E1000_TXD_STAT_DD;

	// Put packet data in buffer
	memset(tx_buffers[tail], 0, PGSIZE);
	memmove(tx_buffers[tail], buf, size);

	// Update tx descriptor
	tx_ring[tail].addr = (uint64_t) va2pa(tx_buffers[tail]); //TODO: Redundant! Put this in the initialization
	tx_ring[tail].length = (uint16_t) size;

	/* Debugging */
	//cprintf("transmit_packet: Transmitting packet from "
	//        "buf=%p, with size=%d\n", buf, size);
	//cprintf("tx_ring[tail].addr = %p\n", tx_ring[tail].addr);
	//cprintf("tx_ring[tail].length = %d\n", tx_ring[tail].length);

	// Update tail
	tail = (tail + 1) % NUM_TX_DESC;
	E1000_REG(E1000_TDT) = tail;
}

// Initializes receive
void
init_receive(void)
{
	cprintf("E1000 initializing receive\n");

	/* Data structures setup */
	// Allocate memory for descriptor ring
	char *va;
	int r;
	if((r = e1000_page_alloc(&va, PTE_P | PTE_W)) < 0)
		panic("e1000_page_alloc: %e", r);
	rx_ring = (struct rx_desc *) va;

	// Allocate memory for the buffers
	int i;
	for (i = 0; i < NUM_RX_DESC; i++) {
		if((r = e1000_page_alloc(&va, PTE_P | PTE_W)) < 0)
			panic("e1000_page_alloc: %e", r);
		rx_buffers[i] = va;
	}

	// Test mappings
	check_rx_mappings(1);
	show_rx_mappings(1);

	// Initial settings of the rx_descriptors
	for (i = 0; i < NUM_RX_DESC; i++) {
		// DD will be set automatically. It starts with 0 for all hardware
		// owned descriptors
		/* Already zero */

		// EOP will be set automatically. It starts with 0 for all hw owned
		// descriptors
		/* Already zero */

		// Buffer address
		rx_ring[i].addr = (uint64_t) va2pa(rx_buffers[i]);
	}

	// The last descriptor starts pointed by the tail
	// The rx descriptor pointed by the tail is always software owned and
	// holds no packet (DD=1, EOP=1)
	rx_ring[NUM_RX_DESC-1].status |= E1000_RXD_STAT_DD;
	rx_ring[NUM_RX_DESC-1].status |= E1000_RXD_STAT_EOP;

	/* Registers setup */

	// RAL0 and RAH0 (stores the 48-bit mac address, for filtering packets)
	// E1000_RAL0 with the low 32 bits
	// E1000_RAH0 with high 16 bits, in bits 0-15. 16-31 are zeroes.
	uint32_t mac_addr_low_32 =
		(((uint32_t) mac_address[0]) <<  0) +
		(((uint32_t) mac_address[1]) <<  8) +
		(((uint32_t) mac_address[2]) << 16) +
		(((uint32_t) mac_address[3]) << 24);
	uint32_t mac_addr_high_16 =
		(((uint32_t) mac_address[4]) << 0) +
		(((uint32_t) mac_address[5]) << 8);
	E1000_REG(E1000_RAL0) = mac_addr_low_32;
	E1000_REG(E1000_RAH0) = mac_addr_high_16;
	E1000_REG(E1000_RAH0) |= E1000_RAH0_AV; // Set E1000_RAH0 Address Valid bit

	// MTA 128 registers (Multicast Table Array)
	// Set everything to zero, as the initial values are X
	uint32_t mta_addr = E1000_MTA;
	for (i = 0; i < 128; i++) {
		E1000_REG(mta_addr) = 0;
		mta_addr += 4;
	}

	// IMS (each bit enables an interrupt)
	// Set to zero as initial value of it's bits are X
	E1000_REG(E1000_IMS) = 0;

	// RDTR (threshold things, not used)
	/* Do nothing */

	// RDBAL and RDBAH (Receive Descriptor Ring address)
	// Always store physical address, not the virtual address!
	// Don't use RDBAH as we are using 32 bit addresses
	E1000_REG(E1000_RDBAL) = va2pa(rx_ring);

	// RDLEN (Receive Descriptor Ring length in bytes)
	// This size must be multiple of 128 bytes
	E1000_REG(E1000_RDLEN) = NUM_RX_DESC * sizeof(struct rx_desc);

	// RDH and RDT (rx_ring head and tail indexes)
	E1000_REG(E1000_RDH) = 0;
	E1000_REG(E1000_RDT) = NUM_RX_DESC - 1;

	// RCTL (Receive Control Register) (Initial values are all 0)
	// RCTL.EN = 1b
	E1000_REG(E1000_RCTL) |= E1000_RCTL_EN;
	// RCTL.LPE = 0b
	/* 0b */
	// RCTL.LBM = 00b
	/* 00b */
	// RCTL.RDMTS = ?
	/* ...? */
	// RCTL.MO = ?
	/* ...? */
	// RCTL.BAM = 1b
	E1000_REG(E1000_RCTL) |= E1000_RCTL_BAM;
	// RCTL.BSIZE = 00b (Buffer size provided by software. 00b->2048 bytes(max))
	/* 00b */
	// RCTL.SECRC = 1b (Strips the CRC from packet)
	E1000_REG(E1000_RCTL) |= E1000_RCTL_SECRC;
}

// Receive packet function. If there is no packet to be received, does nothing.
// The invariants are:
//   Descriptors owned by software: DD and EOP is set
//   Descriptors owned by hardware: DD and EOP are not set
//   The desc. pointed by the tail is SW-owned, but holds no packet.
void
receive_packet(void *buf, size_t *size_store)
{
	// Initial checkings
	if (!buf || !size_store)
		panic("Null pointer passed");

	uint32_t tail = E1000_REG(E1000_RDT);
	uint32_t next = (tail+1)%NUM_RX_DESC;

	// Analyzes if the next is sw owned(DD = 1) or hw owned (DD = 0)
	if (rx_ring[next].status & E1000_RXD_STAT_DD) {
		/* Debugging */
		// cprintf("receive_packet - copying packet to provided buf\n");

		// The next descriptor is sofware owned, so we can read it's data
		// Attention: don't use the buffer address from the descriptor,
		// since it's a physical address
		memmove(buf, rx_buffers[next], (size_t)rx_ring[next].length);
		*size_store = (size_t) rx_ring[next].length;

		// Current tail becomes hw-owned (DD=0, EOP=0)
		rx_ring[tail].status &= ~E1000_RXD_STAT_DD;
		rx_ring[tail].status &= ~E1000_RXD_STAT_EOP;

		// Now make tail point to next
		E1000_REG(E1000_RDT) = next;
	} else {
		// The next descriptor is hardware owned. There's nothing to receive
		/* Do nothing */
		return;
	}
}

uint16_t
read_eeprom(uint32_t addr)
{
	// Order controller to read a byte of mac address
	// EERD.ADDR = address in eeprom space and set EERD.START
	E1000_REG(E1000_EERD) = addr << 8;
	E1000_REG(E1000_EERD) |= E1000_EERD_START;

	// Wait until the reading is done, and then get the data
	while ((E1000_REG(E1000_EERD) & E1000_EERD_DONE) == 0) {
		/* Wait while it's not done */
	}

	// Return the data in EERD.DATA
	return (E1000_REG(E1000_EERD) >> 16);
}

void
read_mac_address(void)
{
	uint16_t mac_address_2_1 = read_eeprom(E1000_EEPROM_ETHERNET_ADDR_2_1);
	uint16_t mac_address_4_3 = read_eeprom(E1000_EEPROM_ETHERNET_ADDR_4_3);
	uint16_t mac_address_6_5 = read_eeprom(E1000_EEPROM_ETHERNET_ADDR_6_5);

	mac_address[0] = (uint8_t) mac_address_2_1;
	mac_address[1] = (uint8_t) (mac_address_2_1 >> 8);
	mac_address[2] = (uint8_t) mac_address_4_3;
	mac_address[3] = (uint8_t) (mac_address_4_3 >> 8);
	mac_address[4] = (uint8_t) mac_address_6_5;
	mac_address[5] = (uint8_t) (mac_address_6_5 >> 8);

	cprintf("MAC address read: %x %x %x %x %x %x\n",
		mac_address[5], mac_address[4], mac_address[3],
		mac_address[2], mac_address[1], mac_address[0]);
}

void
get_mac_address(void *buf)
{
	if (!buf)
		panic("get_mac_address: null pointer");

	uint8_t *mac_addr_copy = (uint8_t *) buf;
	int i;
	for (i = 0; i < 6; i++) {
		*(mac_addr_copy + i) = mac_address[i];
	}
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

	// Read mac address - Challenge
	read_mac_address();

	// Initializations
	init_transmission();
	init_receive();

	/* Debugging */
	//test_transmission();
	//test_receive();

	return 0;
}

/* --------------------------- */
/* --- Auxiliary Functions --- */
/* --------------------------- */

// Custom page allocator for the E1000. If succeeds and va_store is not null,
// stores the virtual address of this new allocated page in it.
// Returns:
//   0 on success
//   -E_NO_MEM if there is no more page to allocate a page or page table for mapping
static int
e1000_page_alloc(char **va_store, int perm)
{
	// Hold the virtual address of the next free page in virtual address space
	static char *nextfree;

	// Initial address should be in the beggining of a free area, above UTOP
	// The chosen address for that was MMIOLIM, there could be a better one
	if (!nextfree) {
		nextfree = (char *) MMIOLIM;
	}

	// Tries to allocate a physical page
	struct PageInfo *pp = page_alloc(ALLOC_ZERO);
	if (!pp) {
	        return -E_NO_MEM;
	}

	// Tries to map the physical page at nextfree on kern_pgdir
	int r;
	if ((r = page_insert(kern_pgdir, pp, nextfree, perm)) < 0) {
	        page_free(pp);
	        return -E_NO_MEM;
	}

	// Store the va of the page
	if(va_store)
		*va_store = nextfree;

	// Increment to next free page, and returns success
	nextfree += PGSIZE;
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
check_tx_mappings(int check)
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
		assert(memcmp(tx_ring, tx_buffers[i], PGSIZE) == 0);
	}

	cprintf("E1000 TX mappings are ok\n");
}

static void
show_tx_mappings(int check)
{
	if (!check) return;

	// Print all the TX mappings
	cprintf("E1000 TX mappings:\n");
	cprintf("\ttx_ring       \tva=%p, \tpa=%p\n", tx_ring, va2pa(tx_ring));
	int i;
	for (i = 0; i < NUM_TX_DESC; i++) {
		cprintf("\ttx_buffers[%d] \tva=%p, \tpa=%p\n",
			i, tx_buffers[i], va2pa(tx_buffers[i]));
	}
}

static void
check_rx_mappings(int check)
{
	if (!check) return;

	// Check that the rx_ring page is present, writable and filled with zeroes
	char *va = (char *) rx_ring;
	*va = 'a';
	assert(*(va)      ==  'a');
	assert(*(va+10)   == '\0');
	assert(*(va+100)  == '\0');
	assert(*(va+1000) == '\0');
	*va = 0;

	// Check that buffers pages are all present, writable and filled with
	// zeroes, by comparing it to the rx_ring page
	int i;
	for (i = 0; i < NUM_RX_DESC; i++) {
		assert(memcmp(rx_ring, rx_buffers[i], PGSIZE) == 0);
	}

	cprintf("E1000 RX mappings are ok\n");
}

static void
show_rx_mappings(int check)
{
	if (!check) return;

	// Print all the RX mappings
	cprintf("E1000 RX mappings:\n");
	cprintf("\trx_ring       \tva=%p, \tpa=%p\n", rx_ring, va2pa(rx_ring));
	int i;
	for (i = 0; i < NUM_RX_DESC; i++) {
		cprintf("\trx_buffers[%d] \tva=%p, \tpa=%p\n",
			i, rx_buffers[i], va2pa(rx_buffers[i]));
	}
}

static void
test_transmission(void)
{
	cprintf("Testing transmission...\n");
	uint32_t data[50];
	int i;

	for (i = 0; i < 50; i++) {
		data[i] = i;
	}

	for (i = 0; i < 18; i++) {
		transmit_packet(&data, sizeof(data));
	}
}

static void
test_receive(void)
{
	cprintf("test_receive - Testing receive...\n");
	char *buf;
	size_t length;
	int r;
	int i;

	// Allocate buffer to hold received packet
	cprintf("test_receive - Allocating buffer to hold received packet\n");
	if ((r = e1000_page_alloc(&buf, PTE_P | PTE_W)) < 0)
		panic("e1000_page_alloc: %e", r);
	cprintf("test_receive - buffer: va = %p, pa = %p\n", buf, va2pa(buf));

	// Try to receive a packet
	cprintf("test_receive - calling receive_packet\n");
	receive_packet(buf, &length);

	// Print the result
	cprintf("test_receive - data on buf after receiving (first 1000 bytes):\n");
	for (i = 0; i < 1000; i++)
		cprintf("%d ", *(buf + i));
	cprintf("\n");
}
