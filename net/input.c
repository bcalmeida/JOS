#include "ns.h"

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
	cprintf("NS INPUT ENV is on!\n");

	// Get Network Server envid
	// TODO:check why envid_t nsenv = ipc_find_env(ENV_TYPE_NS) doesnt work (returns 0)
	// and unhardcode this
	envid_t nsenv = 4097; // HARDCODED!

	// Allocate some pages to receive page
	char *bufs[10];
	char *va = (char *) 0x0ffff000;
	int i;
	for (i = 0; i < 10; i++) {
		sys_page_alloc(0, va, PTE_P | PTE_U | PTE_W);
		bufs[i] = va;
		va += PGSIZE;
	}

	// Infinity loop trying to receive packets and, if received, send it
	// to the network server
	int current_buffer = 0;
	while(1) {
		// Build request
		union Nsipc *nsipc = (union Nsipc *) bufs[current_buffer];
		char *packet_buf = (nsipc->pkt).jp_data;
		size_t size = -1; // Could pass the jp_len instead
		sys_receive_packet(packet_buf, &size);

		// If it receives a packet, the size won't be -1 anymore
		if (size != -1) {
			/* Debugging */
			// cprintf("NS INPUT ENV: Packet received!"
			//         " size = %d\n", size);

			// Store the correct size
			(nsipc->pkt).jp_len = size;

			// Request is built, now send it
			ipc_send(nsenv, NSREQ_INPUT, nsipc, PTE_P|PTE_W|PTE_U);

			// Let the current buffer rest for a while. Go to next.
			current_buffer = (current_buffer + 1)%10;
		}
	}
}
