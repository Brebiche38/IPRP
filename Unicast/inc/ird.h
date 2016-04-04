/**\file ird.h
 * Header file for IRD
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#ifndef __IPRP_IRD_
#define __IPRP_IRD_

#include <stdbool.h>
#include <stdint.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <linux/ip.h>

#include "global.h"
#include "receiver.h"

// Begin cleaned up defines
#define IPRP_IRD_TEXP 120
#define IPRP_IRD_TCLEANUP 5
#define IPRP_DD_MAX_LOST_PACKETS 1024
// End cleaned up defines

/* Typedefs */
typedef struct iprp_receiver_link iprp_receiver_link_t;

/* Thread routines */
void* receive_routine(void* arg);
void* cleanup_routine(void* arg);
int handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data);

/* Utility functions */
iprp_receiver_link_t *receiver_link_get(iprp_header_t *header);
iprp_receiver_link_t *receiver_link_create(iprp_header_t *header);
bool is_fresh_packet(iprp_header_t *packet, iprp_receiver_link_t *link);
uint16_t ip_checksum(struct iphdr *header, size_t len);
uint16_t udp_checksum(uint16_t *packet, size_t len, uint32_t src_addr, uint32_t dest_addr);

/* Receiver link structure */
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

#endif /* __IPRP_IRD_ */