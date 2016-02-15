#ifndef __IPRP_ISD__
#define __IPRP_ISD__

#define IPRP_NFQUEUE_MAX_LENGTH 100
#define IPRP_ISD_TCACHE 3

void send_routine();
void *cache_routine(void *arg);
int handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data);
void cleanup();

#endif /* __IPRP_ISD__ */