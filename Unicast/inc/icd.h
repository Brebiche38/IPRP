/**\file icd.h
 * Header file for ICD
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#ifndef __IPRP_ICD_
#define __IPRP_ICD_

#include "global.h"
#include "sender.h"
#include "receiver.h"

/* Control flow routines */
void* control_routine(void *arg);
void* receiver_routine(void *arg);
void* receiver_sendcap_routine(void *arg);
void* sender_routine(void *arg);

/* Utility functions */
int send_cap(iprp_host_t *sender, int socket);
int send_ack();
int get_cap_message(iprp_ctlmsg_t *msg);
int get_receiver_from_cap(iprp_host_t *recv, iprp_capmsg_t* msg);

/* Sender functions */
int sender_init();
iprp_sender_link_t *peerbase_query(struct in_addr *dest_addr, uint16_t src_port, uint16_t dest_port);
int peerbase_insert(iprp_sender_link_t *link, iprp_host_t *receiver, int inds);
int peerbase_update(iprp_sender_link_t *link);
int peerbase_cleanup(time_t expiration);
uint16_t get_queue_number();

#endif /* __IPRP_ICD_ */