/**\file ird.h
 * Header file for IRD
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#ifndef __IPRP_IRD_
#define __IPRP_IRD_

#include <stdint.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <linux/ip.h>
#include <linux/udp.h>

#include "global.h"
#include "receiver.h"

// Begin cleaned up defines

// End cleaned up defines

#define IPRP_NFQUEUE_MAX_LENGTH 100
#define IPRP_IRD_TCACHE 3

/* Control flow functions */
void receive_routine();
void *cleanup_routine(void* arg);
int handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data);

/* Utility functions */
bool is_fresh_packet(iprp_header_t *packet, iprp_receiver_link_t *link);
uint16_t ip_checksum(struct iphdr *header, size_t len);
uint16_t udp_checksum(uint16_t *packet, size_t len, uint32_t src_addr, uint32_t dest_addr);

#endif /* __IPRP_IRD_ */