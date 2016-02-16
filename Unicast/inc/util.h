/**\file util.h
 * Header file for util.c
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#ifndef __IPRP_UTIL__
#define __IPRP_UTIL__

#include "types.h"

int send_cap(iprp_host_t *sender, int socket);
int send_ack();
int get_cap_message(iprp_ctlmsg_t *msg);
int get_receiver_from_cap(iprp_host_t *recv, iprp_capmsg_t* msg);

int compare_hosts(iprp_host_t *h1, iprp_host_t *h2);
iprp_iface_t *get_iface_from_ind(iprp_host_t *host, iprp_ind_t ind);
int ind_match(iprp_host_t *sender, iprp_host_t *receiver);

/* List structure */
typedef struct list list_t;

struct list {
	void *elem;
	list_t *prev;
	list_t *next;
};
void list_init(list_t *list, void* value);
void list_append(list_t *list, void* value);
// ATTENTION: doubly linked but not cyclical

#define LIST_ITERATE(list, type, var, body) 	\
	void *iterator = (list);					\
	while(iterator != NULL) {					\
		type *var = (type *) iterator->elem;	\
		{ body }								\
		iterator = iterator->next;				\
	}


#endif /* __IPRP_UTIL__ */