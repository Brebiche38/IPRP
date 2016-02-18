#ifndef __IPRP_IRD__
#define __IPRP_IRD__

#include "global.h"
#include "receiver.h"

#define IPRP_NFQUEUE_MAX_LENGTH 100

void receive_routine();
void *cleanup_routine(void* arg);
int handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data);
bool is_fresh_packet(iprp_header_t *packet, iprp_receiver_link_t *link);

uint16_t ip_checksum(struct iphdr *header, size_t len);
uint16_t udp_checksum(uint16_t *packet, size_t len, uint32_t src_addr, uint32_t dest_addr);

#endif /* __IPRP_IRD__ */