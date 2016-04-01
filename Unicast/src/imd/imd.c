#include <errno.h> // errno
#include <stdbool.h> // true, false
#include <stdint.h> // Int types
#include <unistd.h> // sleep
#include <linux/ip.h> // struct iphdr
#include <linux/udp.h> // struct udphdr

#include <netinet/in.h> // Needed for linux/netfilter.h
#include <linux/netfilter.h> // NF_ACCEPT
#include <libnetfilter_queue/libnetfilter_queue.h> // NFQueue functions

#include <pthread.h> // Thread functions
//#include <linux/types.h>
// TODO include atoi, time, calloc, struct in_addr


#include "../../inc/imd.h"
#include "../../inc/global.h"
#include "../../inc/receiver.h"

// Threads
pthread_t monitor_thread;
pthread_t cleanup_thread;

// Active senders list cache (TODO array?)
list_t active_senders;

// NFQueue structures
struct nfq_handle *handle;
struct nfq_q_handle *queue;
int queue_fd;

/**
Moitoring daemon entry point

This method creates the structures for handling the monitoring NFQueue, as well as the threads of the monitoring daemon.
The role of the monitoring daemon is to check all incoming packets on the monitored ports and to update the active senders list for the control deamon to send CAP messages.

\param queue_id The identifier of the NFQueue handling the packets filtered by the monitored ports rules
\return does not return
*/
int main(int argc, char const *argv[]) {
	DEBUG(IPRP_IMD, "Started");
	int err;
	
	// Get arguments
	int queue_id = atoi(argv[1]);

	// Setup nfqueue
	handle = nfq_open();
	if (!handle) {
		ERR("Unable to open queue handle", IPRP_ERR_NFQUEUE);
	}
	if (nfq_unbind_pf(handle, AF_INET) < 0) {
		ERR("Unable to unbind IP protocol from queue handle", IPRP_ERR_NFQUEUE);
	}
	if (nfq_bind_pf(handle, AF_INET) < 0) {
		ERR("Unable to bind IP protocol to queue handle", IPRP_ERR_NFQUEUE);
	}
	queue = nfq_create_queue(handle, queue_id, handle_packet, NULL);
	if (!queue) {
		ERR("Unable to create queue", IPRP_ERR_NFQUEUE);
	}
	if (nfq_set_queue_maxlen(queue, IPRP_NFQUEUE_MAX_LENGTH) == -1) {
		ERR("Unable to set queue max length", IPRP_ERR_NFQUEUE);
	}
	if (nfq_set_mode(queue, NFQNL_COPY_PACKET, 0xffff) == -1) { // TODO why 0xffff and not UINT32_MAX
		ERR("Unable to set queue mode", IPRP_ERR_NFQUEUE);
	}
	queue_fd = nfq_fd(handle); // TODO check for error ?
	DEBUG(IPRP_IMD, "NFQueue created");

	// Launch receiving routine
	if ((err = pthread_create(&monitor_thread, NULL, monitor_routine, NULL))) {
		ERR("Unable to setup monitoring thread", err);
	}
	DEBUG(IPRP_IMD, "Monitor thread created");

	// Launch cleanup routine
	if ((err = pthread_create(&cleanup_thread, NULL, cleanup_routine, NULL))) {
		ERR("Unable to setup cleanup thread", err);
	}
	DEBUG(IPRP_IMD, "Cleanup thread created");

	LOG(IPRP_IMD, "Monitoring daemon successfully created");

	// Join on threads (should not happen)
	void* return_value;
	if ((err = pthread_join(cleanup_thread, &return_value))) {
		ERR("Unable to join on cleanup thread", err);
	}
	ERR("Cleanup thread unexpectedly finished execution", (int) return_value);
	if ((err = pthread_join(monitor_thread, &return_value))) {
		ERR("Unable to join on monitoring thread", err);
	}
	ERR("Monitoring thread unexpectedly finished execution", (int) return_value);

	/* Should not reach this part */
	LOG(IPRP_IMD, "Last man standing at the end of the apocalypse");
	return EXIT_FAILURE;
}

/**
Receives packets from the queue and transfers them to the handle_packet routine.

\return does not return
*/
void* monitor_routine(void* arg) {
	DEBUG(IPRP_IMD_MONITOR, "In routine");

	// Initialize active senders cache
	list_init(&active_senders);
	DEBUG(IPRP_IMD_MONITOR, "Receiver links list initialized");

	// Handle incoming packets
	int bytes;
	char buf[IPRP_PACKET_BUFFER_SIZE];
	while (true) {
		// Receive packet
		if ((bytes = recv(queue_fd, buf, IPRP_PACKET_BUFFER_SIZE, 0)) == -1) {
			if (errno == ENOBUFS) {
				// Queue is losing packets
				// TODO add configuration to see if OK
				LOG(IPRP_IMD_MONITOR, "Too many messages in queue");
				continue;
			}
			ERR("Unable to read packet from queue", errno);
		} else if (bytes == 0) {
			// Receiver has performed an orderly shutdown.
			// TODO Can this happen ? What if yes ?
			DEBUG(IPRP_IMD_MONITOR, "No bytes received");
		}
		DEBUG(IPRP_IMD_MONITOR, "Received packet");

		// Handle packet
		int err;
		if (err = nfq_handle_packet(handle, buf, IPRP_PACKET_BUFFER_SIZE)) {
			//ERR("Error while handling packet", err); // TODO why?
		}
		DEBUG(IPRP_IMD_MONITOR, "Packet handled");
	}
}

/**
Deletes aged entries from the active senders list and pushes the changes down to the file.

\return does not return
*/
void* cleanup_routine(void* arg) {
	DEBUG(IPRP_IMD_CLEANUP, "In routine");

	while(true) {
		// Sample time (for performance)
		time_t curr_time = time(NULL);
		DEBUG(IPRP_IMD_CLEANUP, "Sampled time");

		// Delete aged entries
		int count = 0;
		list_elem_t *iterator = active_senders.head;
		while(iterator != NULL) {
			// Fine grain lock to avoid blocking packet treatment for a long time
			list_lock(&active_senders);

			iprp_active_sender_t *as = (iprp_active_sender_t*) iterator->elem;

			if (curr_time - as->last_seen > IPRP_IMD_TEXP) {
				// Expired sender
				list_elem_t *to_delete = iterator;
				iterator = iterator->next;
				list_delete(&active_senders, to_delete);
				DEBUG(IPRP_IMD_CLEANUP, "Aged entry");
			} else {
				// Good sender
				count++;
				iterator = iterator->next;
				DEBUG(IPRP_IMD_CLEANUP, "Fresh entry");
			}
			list_unlock(&active_senders);
		}
		DEBUG(IPRP_IMD_CLEANUP, "Deleted aged entries");

		// Create active senders file
		iprp_active_sender_t *entries = calloc(count, sizeof(iprp_active_sender_t));
		iterator = active_senders.head;
		for (int i = 0; i < count; ++i) {
			entries[i] = *((iprp_active_sender_t*) iterator->elem);
			iterator = iterator->next;
		}
		DEBUG(IPRP_IMD_CLEANUP, "Created active senders file contents");

		// Update on file
		activesenders_store(IPRP_ACTIVESENDERS_FILE, count, entries);
		LOG(IPRP_IMD_CLEANUP, "Active senders file updated");

		sleep(IPRP_IMD_TCLEANUP);
	}
}

/**
Updates the active senders list with the received packet.

The function creates an active sender entry if it is the first packet it sees from this sender.

\return 0 on success, -1 to end treatment of packets
*/
int handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data) {
	// TODO Check port, launch corresponding stuff
		// Don't give a damn about message
	DEBUG(IPRP_IMD_HANDLE, "Handling packet");

	// Get packet NFQueue header
	struct nfqnl_msg_packet_hdr *nfq_header = nfq_get_msg_packet_hdr(packet); // TODO no error check in IPv6 version
	if (!nfq_header) {
		// TODO just let it go?
		ERR("Unable to retrieve header from received packet", IPRP_ERR_NFQUEUE);
	}
	DEBUG(IPRP_IMD_HANDLE, "Got header");

	// Get packet payload
	unsigned char *buf;
	int bytes;
	if ((bytes = nfq_get_payload(packet, &buf)) == -1) {
		// TODO just let it go ?
		ERR("Unable to retrieve payload from received packet", IPRP_ERR_NFQUEUE);
	}
	DEBUG(IPRP_IMD_HANDLE, "Got payload");

	// Get payload headers and source address
	struct iphdr *ip_header = (struct iphdr *) buf; // TODO assert IP header length = 20 bytes
	struct udphdr *udp_header = (struct udphdr *) (buf + sizeof(struct iphdr)); // TODO sizeof(uint32_t) * ip_header->ip_hdr_len
	struct in_addr src_addr;
	src_addr.s_addr = ntohl(ip_header->saddr);
	uint16_t src_port = ntohs(udp_header->source);
	uint16_t dest_port = ntohs(udp_header->dest);
	DEBUG(IPRP_IMD_HANDLE, "Got IP/UDP headers");

	// Protect the whole process from concurrent cleanup (=> better performance)
	list_lock(&active_senders);

	// Find corresponding entry in active senders (TODO too slow if too much senders, maybe limit sender count => array)
	iprp_active_sender_t *entry = activesenders_find_entry(src_addr, src_port, dest_port);
	DEBUG(IPRP_IMD_HANDLE, "Finished searching for senders");

	// Create entry if not present
	if (!entry) {
		// Create entry
		entry = activesenders_create_entry(src_addr, src_port, dest_port);
		if (!entry) {
			list_unlock(&active_senders);
			ERR("malloc failed to create active senders entry", errno);
		}
		DEBUG(IPRP_IMD_HANDLE, "Active senders entry created");
		
		// Add entry to active senders
		list_append(&active_senders, entry);
		DEBUG(IPRP_IMD_HANDLE, "Active senders entry stored");
	}

	// Update entry timer
	entry->last_seen = time(NULL); // TODO sample every second
	DEBUG(IPRP_IMD_HANDLE, "Entry timer updated");

	// Allow maintenance work to continue
	list_unlock(&active_senders);

	// Accept packet
	if (nfq_set_verdict(queue, ntohl(nfq_header->packet_id), NF_ACCEPT, bytes, buf) == -1) {
		ERR("Unable to set verdict to NF_DROP", IPRP_ERR_NFQUEUE);
	}
	DEBUG(IPRP_IMD_HANDLE, "Packet accepted");

	return 0;
}