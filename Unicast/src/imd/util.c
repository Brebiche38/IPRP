#include <errno.h>
#include <stdint.h>
// TODO include struct in_addr, malloc

#include "../../inc/imd.h"
#include "../../inc/global.h"
#include "../../inc/receiver.h"

extern list_t active_senders;

/**
Find an entry in the active senders cache corresponding to the given parameters

\param src_addr Source IP of the triggering message
\param src_port Source port of the triggering message
\param dest_port Destination port of the triggering message
\return The active sender entry if it exists, NULL otherwise
*/
iprp_active_sender_t *activesenders_find_entry(const struct in_addr src_addr, const uint16_t src_port, const uint16_t dest_port) {
	iprp_active_sender_t *entry = NULL;

	// Find the entry
	list_elem_t *iterator = active_senders.head;
	while(iterator != NULL) {
		iprp_active_sender_t *as = (iprp_active_sender_t*) iterator->elem;
		
		if (src_addr.s_addr == as->src_addr.s_addr) { // TODO really like this (one AS per sender or per port?)
			entry = as;
			break;
		}
		iterator = iterator->next;
	}

	return entry;
}

/**
Creates an active sender entry with the corresponding parameters

\param src_addr Source IP of the triggering message
\param src_port Source port of the triggering message
\param dest_port Destination port of the triggering message
\return On success, the active sender entry is returned, on failure, NULL is returned and errno is set accordingly
*/
iprp_active_sender_t *activesenders_create_entry(const struct in_addr src_addr, const uint16_t src_port, const uint16_t dest_port) {
	iprp_active_sender_t *entry = malloc(sizeof(iprp_active_sender_t));
	if (!entry) {
		return NULL;
	}

	entry->src_addr = src_addr;
	entry->src_port = src_port; // TODO really like this ? (one AS per sender or per port?)
	entry->dest_port = dest_port;

	return entry;
}