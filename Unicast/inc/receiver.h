/**\file receiver.h
 * Header file for lib/receiver.c
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#ifndef __IPRP_RECEIVER_
#define __IPRP_RECEIVER_

#include "global.h"

// Begin cleaned up defines
#define IPRP_ACTIVESENDERS_FILE "files/activesenders.iprp"
// End cleaned up defines

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
IRD: nothing
*/

typedef struct iprp_receiver_link iprp_receiver_link_t;
typedef struct iprp_active_sender iprp_active_sender_t;

struct iprp_active_sender {
	struct in_addr src_addr;
	uint16_t src_port;
	uint16_t dest_port;
	time_t last_seen;
};

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

int activesenders_store(const char* path, int count, iprp_active_sender_t* senders);
int activesenders_load(const char *path, int* count, iprp_active_sender_t** senders);

/* flags */
enum active_senders_flags {
	IPRP_AS_ALL,
	IPRP_AS_NOACK,
	IPRP_AS_ACK
};

#endif /* __IPRP_RECEIVER_ */