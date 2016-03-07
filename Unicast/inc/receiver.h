/**\file receiver.h
 * Header file for lib/receiver.c
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#ifndef __IPRP_RECEIVER_
#define __IPRP_RECEIVER_

#include "global.h"

#define IPRP_ACTIVESENDERS_FILE "files/activesenders.iprp"
#define IPRP_ACTIVESENDERS_LINE_LENGTH 80
#define IPRP_ACTIVESENDERS_MAX_SENDERS 256

#define IPRP_DD_MAX_LOST_PACKETS 1024

/**
2 files: active senders (shared receiver-side) and monitored ports (ICD only)

Monitored ports: read-only (except startup?). From ICD. No caching.
On add: add rule to IMD queue. On delete: remove rule to IMD queue.
First add -> create IMD/IRD
Count to 0 -> delete IMD/IRD

Active senders: internal to iPRP, holds src_addr, src_port, (dest_port?), last seen
ICD: retrieves to send CAPs (read-only, no latency requirements)
IMD: updates keep-alive, deletes aged entries (deletion can be periodical, update must be fast -> caching)
*/

typedef struct iprp_receiver_link iprp_receiver_link_t;

typedef struct iprp_active_sender {
	iprp_host_t host;
	uint32_t last_seen;
} iprp_active_sender_t;

struct iprp_receiver_link {
	// Info (fixed) vars
	struct in_addr src_addr;
	uint16_t src_port;
	unsigned char snsid[20];
	// State (variable) vars
	uint32_t list_sn[IPRP_DD_MAX_LOST_PACKETS];
	uint32_t high_sn;
	time_t last_seen;
};

/* Library functions */
int receiver_init();
int get_active_senders(iprp_host_t *buf, size_t size, int flags);
//int is_active_sender(iprp_sender_t *sender);
//void set_sender_ack(iprp_sender_t *sender);

/* Utility functions */
int parse_as_entry(char *line, iprp_active_sender_t *entry);
int get_as_entry(char *line, iprp_active_sender_t *entry);
int find_sender(iprp_host_t *sender, iprp_host_t *buf);

/* flags */
enum active_senders_flags {
	IPRP_AS_ALL,
	IPRP_AS_NOACK,
	IPRP_AS_ACK
};

#endif /* __IPRP_RECEIVER_ */