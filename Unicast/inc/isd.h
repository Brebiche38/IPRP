/**\file isd.h
 * Header file for ISD
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#ifndef __IPRP_ISD_
#define __IPRP_ISD_

// Begin cleaned up defines

// End cleaned up defines

#define IPRP_ISD_TCACHE 3

void send_routine();
void *cache_routine(void *arg);
int handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data);
void cleanup();

#endif /* __IPRP_ISD_ */