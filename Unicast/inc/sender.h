/**\file sender.h
 * Header file for sender.c
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#ifndef __IPRP_SENDER_
#define __IPRP_SENDER_

// Sender-side control interface

#include <time.h>
#include <stdbool.h>

#include "types.h"

#define IPRP_PEERBASE_MAX_SIZE (IPRP_PEERBASE_1STLINE_LENGTH + IPRP_MAX_IFACE * IPRP_PEERBASE_LINE_LENGTH)
#define IPRP_PEERBASE_1STLINE_LENGTH 80
#define IPRP_PEERBASE_LINE_LENGTH 80

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

/* Library functions */
int sender_init();
iprp_sender_link_t *peerbase_query(iprp_capmsg_t *cap, struct in_addr *dest_addr);
int peerbase_insert(iprp_sender_link_t *link, iprp_host_t *receiver, int inds);
int peerbase_update(iprp_sender_link_t *link);

/* Disk functions */
int peerbase_store(const char* path, iprp_peerbase_t *base);
int peerbase_load(const char* path, iprp_peerbase_t *base);

/* Utility functions */

/*
 Peer base file format
 ?? 1st line: link description (maybe): identifier, dest IP, src/dest ports, queue id
 Next lines: 1 connection per line: IND, source IP ?, dest IP, interface name ? 
 */

#endif /* __IPRP_SENDER_ */