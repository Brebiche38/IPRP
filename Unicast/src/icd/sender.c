#include <errno.h>
#include <string.h>

#include "../../inc/icd.h"
#include "../../inc/global.h"
#include "../../inc/sender.h"

extern iprp_host_t this;

list_t current_links; // sender_link_t
// TODO mutex

static volatile uint16_t queue_number = 0;

int sender_init() { // TODO remove arg with bootstrap
	list_init(&current_links);

	return 0;
}

iprp_sender_link_t *peerbase_query(iprp_capmsg_t *cap, struct in_addr *dest_addr) {
	if (!cap || !dest_addr) return NULL;

	list_elem_t *iterator = current_links.head;

	while(iterator) {
		iprp_sender_link_t *link = (iprp_sender_link_t *) iterator->elem;
		if (link->dest_addr.s_addr == dest_addr->s_addr && link->src_port == cap->src_port && link->dest_port == cap->src_port) {
			return link;
		}
		iterator = iterator->next;
	}

	return NULL;
}

int peerbase_insert(iprp_sender_link_t *link, iprp_host_t *receiver, int inds) {
	// 1. Create peer base
	iprp_peerbase_t peerbase;
	memset(&peerbase, 0, sizeof(iprp_peerbase_t));

	peerbase.link = *link;

	printf("receiver interfaces: %d\n", receiver->nb_ifaces);

	for (int i = 0; i < receiver->nb_ifaces; ++i) {
		int ind = receiver->ifaces[i].ind;
		if (peerbase.paths[ind].active) {
			return IPPR_ERR_MULTIPLE_SAME_IND;
		}

		if (inds & (1 << ind)) {
			printf("IND match for %x\n", ind);
			iprp_iface_t *iface = get_iface_from_ind(&this, ind);

			if (iface) {
				peerbase.paths[ind].active = true;
				peerbase.paths[ind].iface = *iface;
				peerbase.paths[ind].dest_addr = receiver->ifaces[i].addr;
			} else {
				printf("No IND for %x\n", ind);
			}
		}
	}

	peerbase.link.last_cap = time(NULL);

	// TODO 1. Create file for sender deamon
	char path[IPRP_PATH_LENGTH];
	snprintf(path, IPRP_PATH_LENGTH, "files/base_%x.iprp", link->receiver_id);
	peerbase_store(path, &peerbase);

	// TODO 2. Insert in current links list
	list_append(&current_links, link);

	return 0;
}

int peerbase_update(iprp_sender_link_t *link) {
	iprp_peerbase_t peerbase;
	char path[IPRP_PATH_LENGTH];
	snprintf(path, IPRP_PATH_LENGTH, "files/base_%x.iprp", link->receiver_id);

	int err;

	if ((err = peerbase_load(path, &peerbase))) {
		ERR("Unable to load peerbase", err);
	}

	peerbase.link.last_cap = time(NULL);

	if ((err = peerbase_store(path, &peerbase))) {
		ERR("Unable to store peerbase", err);
	}

	return 0;
}

int peerbase_cleanup(time_t expiration) {
	// TODO lock
	list_elem_t *iterator = current_links.head;
	while(iterator) {
		iprp_sender_link_t *link = (iprp_sender_link_t *) iterator->elem;
		if (link->last_cap < expiration) {
			// Delete corresponding connection
			// send SIGTERM to ISD, need to know pid
			// TODO kill(link->isd_pid, SIGTERM);
		}
	}
	return 0;
}

uint16_t get_queue_number() {
	bool ok = false;
	uint16_t num;

	srand(time(0));
	while(!ok) {
		num = (uint16_t) (rand() % 1024);
		printf("num: %u\n", num);
		list_elem_t *iterator = current_links.head;
		while(iterator) {
			iprp_sender_link_t *link = iterator->elem;
			if (link->queue_id == num) {
				ok = false;
				break;
			}
			iterator = iterator->next;
		}

		ok = true;
	}

	return num;
}