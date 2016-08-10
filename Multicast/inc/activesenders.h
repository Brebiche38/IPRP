/**\file receiver.h
 * Header file for lib/receiver.c
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#ifndef __IPRP_RECEIVER_
#define __IPRP_RECEIVER_

#include <stdbool.h>

#include "global.h"

// Begin cleaned up defines
#define IPRP_AS_FILE "files/activesenders.iprp"
// End cleaned up defines

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

typedef struct {
	struct in_addr src_addr;
	struct in_addr dest_group;
	uint16_t src_port;
	uint16_t dest_port;
	time_t last_seen;
	bool iprp_enabled;
} iprp_active_sender_t;

void activesenders_store(const char* path, const int count, const iprp_active_sender_t* senders);
int activesenders_load(const char *path, int* count, iprp_active_sender_t** senders);

#endif /* __IPRP_RECEIVER_ */