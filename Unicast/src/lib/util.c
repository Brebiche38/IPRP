/**\file util.c
 * Utility functions
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#include <stdlib.h>
#include <stdio.h>

#include "../../inc/util.h"

extern iprp_host_t this;

int send_cap(iprp_host_t *sender, int socket) {
	/* TODO stub */
	iprp_ctlmsg_t msg;

	msg.secret = IPRP_CTLMSG_SECRET;
	msg.msg_type = IPRP_CAP;
	
	msg.message.cap_message.iprp_version = IPRP_VERSION;
	msg.message.cap_message.receiver = this;
	msg.message.cap_message.src_port = 0;
	msg.message.cap_message.dest_port = 0;

	// Send message
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(IPRP_CTL_PORT);
	addr.sin_addr = sender->ifaces[0].addr;

	sendto(socket, (void*) &msg, sizeof(msg), 0, (struct sockaddr*) &addr, sizeof(addr));

	return 0;
}

// No unicast
int send_ack() {
	/* TODO stub */
	return 0;
}

int get_receiver_from_cap(iprp_host_t *recv, iprp_capmsg_t* msg) {
	*recv = msg->receiver;
	return 0;
}

int compare_hosts(iprp_host_t *h1, iprp_host_t *h2) {
	for (int i = 0; i < IPRP_MAX_IFACE; ++i) {
		if (h1->ifaces[i].addr.s_addr != h2->ifaces[i].addr.s_addr || h1->ifaces[i].ind != h2->ifaces[i].ind) {
			return 0;
		}
	}
	return 1;
}

iprp_iface_t *get_iface_from_ind(iprp_host_t *host, iprp_ind_t ind) {
	for (int i = 0; i < host->nb_ifaces; ++i) {
		if (host->ifaces[i].ind == ind) {
			return &host->ifaces[i];
		}
	}

	return NULL;
}

int ind_match(iprp_host_t *sender, iprp_host_t *receiver) {
	int matching_inds = 0;

	for (int i = 0; i < IPRP_MAX_IFACE; ++i) {
		if (sender->ifaces[i].ind == receiver->ifaces[i].ind) {
			matching_inds |= (1 << sender->ifaces[i].ind);
		}
	}

	return matching_inds;
}

void list_init(list_t *list, void* value) {
	list->elem = value;
	list->prev = NULL;
	list->next = NULL;
}

void list_append(list_t *list, void* value) {
	if (list->elem == NULL) {
		list->elem = value;
	} else {
		list_t *new_elem = malloc(sizeof(list_t));
		new_elem->elem = value;
		new_elem->prev = list;
		new_elem->next = list->next;
		if (list->next) {
			list->next->prev = new_elem;
		}
		list->next = new_elem;		
	}
}