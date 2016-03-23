#include <stdint.h>
#include <pthread.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/netfilter.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <stdbool.h>
#include <errno.h>
#include <linux/ip.h>
#include <linux/udp.h>

#include "../../inc/imd.h"
#include "../../inc/global.h"
#include "../../inc/receiver.h"

pthread_t cleanup_thread;

list_t active_senders;

struct nfq_handle *handle;
struct nfq_q_handle *queue;
int queue_fd;

int main(int argc, char const *argv[]) {
	LOG("[imd] started");
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

	LOG("[imd] nfqueue created");

	// Launch cleanup routine (here?)
	int err;
	if ((err = pthread_create(&cleanup_thread, NULL, cleanup_routine, NULL))) {
		ERR("Unable to setup cleanup thread", err);
	}
	LOG("[imd] cleanup thread created");

	// Launch receiving routine
	monitor_routine();

	LOG("[imd] out of receive routine. This should not happen");
	return EXIT_FAILURE;
}

void monitor_routine() {
	LOG("[imd-monitor] in routine");

	list_init(&receiver_links);
	LOG("[imd-monitor] Receiver links list initialized");

	int bytes;
	char buf[IPRP_PACKET_BUFFER_SIZE];

	while (true) {
		if ((bytes = recv(queue_fd, buf, IPRP_PACKET_BUFFER_SIZE, 0)) == -1) {
			if (errno == ENOBUFS) {
				// TODO add configuration to see if OK
				LOG("[imd-monitor] too many messages in queue");
				continue;
			}
			ERR("Unable to read packet from queue", errno);
		} else if (bytes == 0) {
			// Receiver has performed an orderly shutdown.
			// TODO Can this happen ? What if yes ?
			LOG("[imd-monitor] No bytes received");
		}

		LOG("[imd-monitor] received packet");

		int err;
		if (err = nfq_handle_packet(handle, buf, IPRP_PACKET_BUFFER_SIZE)) {
			//ERR("Error while handling packet", err); // TODO why?
		}

		LOG("[imd-monitor] packet handled");
	}

	LOG("[imd-monitor] out of loop");
}

void* cleanup_routine(void* arg) {
	// Delete aged entries
	time_t curr_time = time(NULL);

	int count = 0;

	list_elem_t *iterator = active_senders.head;
	while(iterator != NULL) {
		iprp_active_sender_t *as = (iprp_active_sender_t*) iterator->elem;

		iterator = iterator->next;
		if (curr_time - as->last_seen > IPRP_IMD_TCLEANUP) {
			list_delete(active_senders, as);
		} else {
			count++;
		}
	}

	iprp_active_sender_t *entries = calloc(count, sizeof(iprp_active_sender_t));
	iterator = active_senders.head;
	for (int i = 0; i < count; ++i) {
		entries[i] = (iprp_active_sender_t*) iterator->elem;
		iterator = iterator->next;
	}

	// Update on file
	activesenders_store(IPRP_ACTIVESENDERS_FILE, count, entries);
}

int handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data) {
	// TODO Check port, launch corresponding stuff
		// Don't give a damn about message
	LOG("[imd-handle] Handling packet");

	// 1. Get header and payload
	struct nfqnl_msg_packet_hdr *nfq_header = nfq_get_msg_packet_hdr (packet); // TODO no error check in IPv6 version
	if (!nfq_header) {
		// TODO just let it go?
		ERR("Unable to retrieve header from received packet", IPRP_ERR_NFQUEUE);
	}
	LOG("[imd-handle] Got header");

	unsigned char *buf;
	int bytes;
	if ((bytes = nfq_get_payload(packet, &buf)) == -1) {
		// TODO just let it go ?
		ERR("Unable to retrieve payload from received packet", IPRP_ERR_NFQUEUE);
	}
	LOG("[imd-handle] Got payload");

	// Get payload headers
	struct iphdr *ip_header = (struct iphdr *) buf; // TODO assert IP header length = 20 bytes
	struct udphdr *udp_header = (struct udphdr *) (buf + sizeof(struct iphdr)); // TODO sizeof(uint32_t) * ip_header->ip_hdr_len

	LOG("[imd-handle] Got to interesting part");

	struct in_addr src_addr = ntohl(ip_header->ip_src);

	// Find corresponding entry in active senders
	iprp_active_sender_t *entry = NULL;
	list_elem_t *iterator = active_senders.head;
	while(iterator != NULL) {
		iprp_active_sender_t *as = (iprp_active_sender_t*) iterator->elem;
		
		if (src_addr.saddr == as->src_addr.saddr) {
			entry = as;
			break;
		}
		iterator = iterator->next;
	}

	if (!entry) {
		// TODO Create it
		entry = malloc(sizeof(iprp_active_sender_t));
		if (!entry) {
			ERR("malloc failed to create active senders entry", errno);
		}

		entry->src_addr = src_addr;
		
		// Add entry to active senders
		list_append(&active_senders, entry);
	}

	entry->last_seen = time(NULL);

	// Accept packet
	if (nfq_set_verdict(queue, ntohl(nfq_header->packet_id), NF_ACCEPT, bytes, buf) == -1) {
		ERR("Unable to set verdict to NF_DROP", IPRP_ERR_NFQUEUE);
	}

	return 0;
}