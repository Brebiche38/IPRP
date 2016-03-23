#ifndef __IPRP_IMD_
#define __IPRP_IMD_

#define IPRP_IMD_TCLEANUP 5
#define IPRP_IMD_TEXP 120

void monitor_routine();
void* cleanup_routine(void* arg);
int handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data);

#endif /* __IPRP_IMD_ */