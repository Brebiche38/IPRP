#define IPRP_FILE IMD_AS

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "imd.h"
#include "activesenders.h"

extern time_t curr_time;

list_t active_senders;

// Function prototypes
int count_and_cleanup();
iprp_active_sender_t *get_entries(int count);

/**
Deletes aged entries from the active senders list and pushes the changes down to the file.

\return does not return
*/
void* as_routine(void* arg) {
	DEBUG("In routine");

	while(true) {
		// Delete aged entries
		int count = count_and_cleanup();
		DEBUG("Deleted aged entries");

		// Create active senders file
		iprp_active_sender_t *entries = get_entries(count);
		DEBUG("Created active senders file contents");

		// Update on file
		activesenders_store(IPRP_AS_FILE, count, entries);
		LOG("Active senders file updated");

		sleep(IMD_T_AS_CACHE);
	}
}

int count_and_cleanup() {
	int count = 0;
	list_elem_t *iterator = active_senders.head;
	while(iterator != NULL) {
		// Fine grain lock to avoid blocking packet treatment for a long time
		list_lock(&active_senders);

		iprp_active_sender_t *as = (iprp_active_sender_t*) iterator->elem;
		if (curr_time - as->last_seen > IMD_AS_TEXP) {
			// Expired sender
			list_elem_t *to_delete = iterator;
			iterator = iterator->next;
			list_delete(&active_senders, to_delete);
			DEBUG("Aged entry");
		} else {
			// Good sender
			count++;
			iterator = iterator->next;
			DEBUG("Fresh entry");
		}
		list_unlock(&active_senders);
	}
	return count;
}

iprp_active_sender_t *get_entries(int count) {
	iprp_active_sender_t *entries = calloc(count, sizeof(iprp_active_sender_t));
	if (!entries) {
		ERR("Unable to allocate active senders entries", errno);
	}
	list_elem_t *iterator = active_senders.head;
	for (int i = 0; i < count; ++i) {
		entries[i] = *((iprp_active_sender_t*) iterator->elem);
		iterator = iterator->next;
	}
	return entries;
}