/**\file sender.c
 * Sender-side logic (Sender daemons, peer bases)
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#include "../../inc/global.h"
#include "../../inc/sender.h"

int peerbase_store(const char* path, iprp_peerbase_t *base) {
	if (path == NULL | base == NULL) return IPRP_ERR_NULLPTR;

	// Save file
	FILE* writer = fopen(path, "w");
	if (!writer) {
		ERR("Unable to write to peerbase file", errno);
	}

	fwrite(base, sizeof(iprp_peerbase_t), 1, writer);

	fflush(writer);		

	fclose(writer);

	return 0;
}

int peerbase_load(const char *path, iprp_peerbase_t *base) {
	if (path == NULL || base == NULL) return IPRP_ERR_NULLPTR;

	FILE* reader = fopen(path, "r");
	if (!reader) {
		ERR("Unable to read peerbase file", errno);
	}

	fread(base, sizeof(iprp_peerbase_t), 1, reader);

	fclose(reader);

	return 0;
}

void peerbase_print(iprp_peerbase_t *base) {
	printf("Addr/ports: %x %u %u\n", base->link.dest_addr.s_addr, base->link.src_port, base->link.dest_port);
	printf("Receiver ID: %x, Queue ID: %d, ISD PID: %d, Last CAP: %d\n", base->link.receiver_id, base->link.queue_id, base->link.isd_pid, base->link.last_cap);
	for (int i = 0; i < IPRP_MAX_INDS; ++i) {
		if (base->paths[i].active) {
			printf("Interface %x (active): from %x to %x\n", i, base->paths[i].iface.addr.s_addr, base->paths[i].dest_addr.s_addr);
		} else {
			printf("Interface %x (inactive): from %x to %x\n", i, base->paths[i].iface.addr.s_addr, base->paths[i].dest_addr.s_addr);
		}
	}
}