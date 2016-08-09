#define IPRP_FILE ICD_SI

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "icd.h"
#include "senderifaces.h"

extern time_t curr_time;

list_t sender_ifaces;

// Function prototypes
int count_and_cleanup();
iprp_sender_ifaces_t *get_file_contents();

void *si_routine(void *arg) {
	DEBUG("In routine");

	list_init(&sender_ifaces);
	senderifaces_store(IPRP_SI_FILE, 0, NULL);
	DEBUG("Sender interfaces initialized");

	while(true) {
		// Delete aged entries
		int count = count_and_cleanup();
		DEBUG("Deleted aged entries");

		// Create active senders file
		iprp_sender_ifaces_t *entries = get_file_contents(count);
		DEBUG("Created sender interfaces file contents");

		// Update on file
		senderifaces_store(IPRP_SI_FILE, count, entries);
		LOG("Active senders file updated");

		sleep(IPRP_T_SI_CACHE);
	}
}

int count_and_cleanup() {
	int count = 0;
	list_elem_t *iterator = sender_ifaces.head;
	while(iterator != NULL) {
		// Fine grain lock to avoid blocking packet treatment for a long time
		list_lock(&sender_ifaces);

		iprp_sender_ifaces_t *si = (iprp_sender_ifaces_t*) iterator->elem;
		if (curr_time - si->last_seen > ICD_SI_TEXP) {
			// Expired sender
			list_elem_t *to_delete = iterator;
			iterator = iterator->next;
			free(to_delete->elem);
			list_delete(&sender_ifaces, to_delete);
			DEBUG("Aged entry");
		} else {
			// Good sender
			count++;
			iterator = iterator->next;
			DEBUG("Fresh entry");
		}
		list_unlock(&sender_ifaces);
	}
	return count;
}

iprp_sender_ifaces_t *get_file_contents(int count) {
	iprp_sender_ifaces_t *entries = calloc(count, sizeof(iprp_sender_ifaces_t));
	if (!entries) {
		ERR("Unable to allocate file contents", errno);
	}

	list_elem_t *iterator = sender_ifaces.head;
	for (int i = 0; i < count; ++i) {
		entries[i] = *((iprp_sender_ifaces_t*) iterator->elem);
		iterator = iterator->next;
	}

	return entries;
}