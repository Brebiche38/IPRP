/**\file global.c
 * Utility functions
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "../../inc/global.h"

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

	for (int i = 0; i < sender->nb_ifaces; ++i) {
		if (sender->ifaces[i].ind == receiver->ifaces[i].ind) {
			matching_inds |= (1 << sender->ifaces[i].ind);
		}
	}

	return matching_inds;
}

void list_init(list_t *list) {
	list->head = NULL;
	list->tail = NULL;
	list->size = 0;
	list->mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
}

void list_append(list_t *list, void* value) {
	list_elem_t *new_elem = malloc(sizeof(list_elem_t));
	if (!new_elem) {
		ERR("Unable to allocate list element", errno);
	}

	new_elem->elem = value;
	new_elem->next = NULL;
	new_elem->prev = list->tail;

	if (list->head == NULL) { // Implicitly, list->tail == NULL too
		list->head = new_elem;
	} else {
		list->tail->next = new_elem;
	}
	list->tail = new_elem;

	list->size++;
}

void list_delete(list_t *list, list_elem_t *elem) {
	if (list->head == elem) {
		list->head = elem->next;
	}
	if (list->tail == elem) {
		list->tail = elem->prev;
	}

	// Global case
	if (elem->next) {
		elem->next->prev = elem->prev;
	}
	if (elem->prev) {
		elem->prev->next = elem->next;
	}

	list->size--;

	free(elem);
}

size_t list_size(list_t *list) {
	return list->size;
}

void list_lock(list_t *list) {
	pthread_mutex_lock(&list->mutex);
}

void list_unlock(list_t *list) {
	pthread_mutex_unlock(&list->mutex);
}

char* iprp_thr_name(iprp_thread_t thread) {
	switch(thread) {
		case IPRP_ICD: return "icd";
		case IPRP_ICD_CONTROL: return "icd-control";
		case IPRP_ICD_RECV: return "icd-receiver";
		case IPRP_ICD_SEND: return "icd-sender";
		case IPRP_ICD_SENDCAP: return "icd-sendcap";
		case IPRP_ICD_PORTS: return "icd-ports";

		case IPRP_ISD: return "isd";
		case IPRP_ISD_SEND: return "isd-send";
		case IPRP_ISD_CACHE: return "isd-cache";
		case IPRP_ISD_HANDLE: return "isd-handle";

		case IPRP_IMD: return "imd";
		case IPRP_IMD_MONITOR: return "imd-monitor";
		case IPRP_IMD_CLEANUP: return "imd-cleanup";
		case IPRP_IMD_HANDLE: return "imd-handle";

		case IPRP_IRD: return "ird";
		case IPRP_IRD_RECV: return "ird-recv";
		case IPRP_IRD_CLEANUP: return "ird-cleanup";
		case IPRP_IRD_HANDLE: return "ird-handle";

		default: return "???";
	}	
}