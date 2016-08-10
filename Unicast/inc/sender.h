/**\file sender.h
 * Header file for lib/sender.c
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#ifndef __IPRP_SENDER_
#define __IPRP_SENDER_

#include <time.h>
#include <stdbool.h>

#include "global.h"

// Begin cleaned up defines

// End cleaned up defines

/* Structures */
// Definitions : a link is attached to each sender deamon, each link has multiple paths over which the data is replicated
typedef struct iprp_peerbase iprp_peerbase_t;
typedef struct iprp_sender_link iprp_sender_link_t;
typedef struct iprp_path iprp_path_t;

struct iprp_sender_link {
	struct in_addr dest_addr;
	uint16_t src_port;
	uint16_t dest_port;
	uint32_t receiver_id; // necessary
	uint16_t queue_id;
	pid_t isd_pid;
	time_t last_cap;
};

struct iprp_path {
	bool active;
	iprp_iface_t iface;
	struct in_addr dest_addr;
};

struct iprp_peerbase {
	iprp_sender_link_t link;
	iprp_path_t paths[IPRP_MAX_INDS];
};

/* Disk functions */
int peerbase_store(const char* path, iprp_peerbase_t *base);
int peerbase_load(const char* path, iprp_peerbase_t *base);
void peerbase_print(iprp_peerbase_t *base);

#endif /* __IPRP_SENDER_ */