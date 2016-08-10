#ifndef __IPRP_IMD_
#define __IPRP_IMD_

#include <stdint.h>
#include <libnetfilter_queue/libnetfilter_queue.h>

#include "global.h"
#include "receiver.h"

// Begin cleaned up defines
#define IPRP_IMD_TCLEANUP 5
#define IPRP_IMD_TEXP 120
// End cleaned up defines

/* Thread routines */
void* monitor_routine(void* arg);
void* cleanup_routine(void* arg);
int handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data);

/* Utility functions */
iprp_active_sender_t *activesenders_find_entry(struct in_addr src_addr, uint16_t src_port, uint16_t dest_port);
iprp_active_sender_t *activesenders_create_entry(struct in_addr src_addr, uint16_t src_port, uint16_t dest_port);

#endif /* __IPRP_IMD_ */