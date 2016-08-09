#define IPRP_FILE IMD_HANDLE

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <linux/ip.h>
#include <linux/udp.h>
// #include nfqueue

#include "imd.h"

extern time_t curr_time;
extern list_t active_senders;

// Function prototypes
int handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data);
int ird_handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data);
int global_handle(struct nfq_q_handle *queue, struct nfq_data *packet, bool iprp_message);
iprp_active_sender_t *activesenders_find_entry(const struct in_addr src_addr, const struct in_addr group_addr, const uint16_t src_port, const uint16_t dest_port);
iprp_active_sender_t *activesenders_create_entry(const struct in_addr src_addr, const struct in_addr group_addr, const uint16_t src_port, const uint16_t dest_port, const bool iprp_enabled);

/**
Receives packets from the queue and transfers them to the handle_packet routine.

\return does not return
*/
void* handle_routine(void* arg) {
	uint16_t queue_id = (uint16_t) arg;
	DEBUG("In routine");

	// Setup NFQueue
	iprp_queue_t nfq;
	queue_setup(&nfq, queue_id, handle_packet);
	DEBUG("NFQueue setup (%d)", queue_id);

	// Handle outgoing packets
	while (true) {
		// Get packet
		int err = get_and_handle(nfq.handle, nfq.fd);
		if (err) {
			if (err == IPRP_ERR) {
				ERR("Unable to retrieve packet from IMD queue", errno);
			}
			DEBUG("Error %d while handling packet", err);
		}
		DEBUG("Packet handled");
	}
}

void* ird_handle_routine(void* arg) {
	uint16_t queue_id = (uint16_t) arg;
	DEBUG("IRD In routine");

	// Setup NFQueue
	iprp_queue_t nfq;
	queue_setup(&nfq, queue_id, ird_handle_packet);
	DEBUG("IRD NFQueue setup (%d)", queue_id);

	// Handle outgoing packets
	while (true) {
		// Get packet
		int err = get_and_handle(nfq.handle, nfq.fd);
		if (err) {
			if (err == IPRP_ERR) {
				ERR("Unable to retrieve packet from IMD-IRD queue", errno);
			}
			DEBUG("Error %d while handling packet", err);
		}
		DEBUG("Packet handled");
	}
}


/**
Updates the active senders list with the received packet.

The function creates an active sender entry if it is the first packet it sees from this sender.

\return 0 on success, -1 to end treatment of packets
*/
int handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data) {
	DEBUG("Handling packet");

	return global_handle(queue, packet, false);
}

// Handle packet coming from the IRD
int ird_handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data) {
	DEBUG("Handling packet");

	return global_handle(queue, packet, true);
}

int global_handle(struct nfq_q_handle *queue, struct nfq_data *packet, bool iprp_message) {
	// Get packet payload
	int bytes;
	unsigned char *buf;
	if ((bytes = nfq_get_payload(packet, &buf)) == -1) {
		// TODO just let it go ?
		ERR("Unable to retrieve payload from received packet", IPRP_ERR_NFQUEUE);
	}
	DEBUG("Got payload");

	// Get payload headers and source address
	struct iphdr *ip_header = (struct iphdr *) buf; // TODO assert IP header length = 20 bytes
	struct udphdr *udp_header = (struct udphdr *) (buf + sizeof(struct iphdr)); // TODO sizeof(uint32_t) * ip_header->ip_hdr_len
	DEBUG("Got IP/UDP headers");
	
	// Get link information
	struct in_addr src_addr;
	src_addr.s_addr = ip_header->saddr;
	struct in_addr group_addr;
	group_addr.s_addr = ip_header->daddr;
	uint16_t src_port = ntohs(udp_header->source);
	uint16_t dest_port = ntohs(udp_header->dest);
	DEBUG("Got link information");

	// Protect the whole process from concurrent cleanup (=> better performance)
	list_lock(&active_senders);

	// Find corresponding entry in active senders (TODO too slow if too much senders, maybe limit sender count => array)
	iprp_active_sender_t *entry = activesenders_find_entry(src_addr, group_addr, src_port, dest_port);
	DEBUG("Finished searching for senders: src %x:%u, dst %x:%u", src_addr.s_addr, src_port, group_addr.s_addr, dest_port);

	// Create entry if not present
	if (!entry) {
		// Create entry
		entry = activesenders_create_entry(src_addr, group_addr, src_port, dest_port, iprp_message);
		if (!entry) {
			list_unlock(&active_senders);
			ERR("malloc failed to create active senders entry", errno);
		}
		DEBUG("Active senders entry created");
		
		// Add entry to active senders
		list_append(&active_senders, entry);
		DEBUG("Active senders entry stored");
	}
	
	// Update entry timer
	entry->last_seen = curr_time;
	if (iprp_message) {
		entry->iprp_enabled = true;
	}
	DEBUG("Entry timer updated");

	// Allow maintenance work to continue
	list_unlock(&active_senders);

	// Accept of reject packet accordingly
	struct nfqnl_msg_packet_hdr *nfq_header = nfq_get_msg_packet_hdr(packet); // TODO no error check in IPv6 version
	if (!nfq_header) {
		ERR("Unable to retrieve header from received packet", IPRP_ERR_NFQUEUE);
	}
	DEBUG("Got header");
	uint32_t verdict = (iprp_message || !entry->iprp_enabled) ? NF_ACCEPT : NF_DROP;
	if (nfq_set_verdict(queue, ntohl(nfq_header->packet_id), verdict, bytes, buf) == -1) {
		ERR("Unable to set verdict to NF_DROP", IPRP_ERR_NFQUEUE);
	}
	LOG((verdict == NF_ACCEPT) ? "Packet accepted" : "Packet dropped");

	return 0;
}

/**
Find an entry in the active senders cache corresponding to the given parameters

\param src_addr Source IP of the triggering message
\param src_port Source port of the triggering message
\param dest_port Destination port of the triggering message
\return The active sender entry if it exists, NULL otherwise
*/
iprp_active_sender_t *activesenders_find_entry(const struct in_addr src_addr, const struct in_addr group_addr, const uint16_t src_port, const uint16_t dest_port) {
	// Find the entry
	list_elem_t *iterator = active_senders.head;
	while(iterator != NULL) {
		iprp_active_sender_t *as = (iprp_active_sender_t*) iterator->elem;
		
		if (src_addr.s_addr == as->src_addr.s_addr &&
				group_addr.s_addr == as->dest_group.s_addr &&
				//src_port == as->src_port &&
				dest_port == as->dest_port) { // TODO really like this (one AS per sender or per port?)
			return as;
		}
		iterator = iterator->next;
	}
	return NULL;
}

/**
Creates an active sender entry with the corresponding parameters

\param src_addr Source IP of the triggering message
\param src_port Source port of the triggering message
\param dest_port Destination port of the triggering message
\return On success, the active sender entry is returned, on failure, NULL is returned and errno is set accordingly
*/
iprp_active_sender_t *activesenders_create_entry(const struct in_addr src_addr, const struct in_addr group_addr, const uint16_t src_port, const uint16_t dest_port, const bool iprp_enabled) {
	iprp_active_sender_t *entry = malloc(sizeof(iprp_active_sender_t));
	if (!entry) {
		return NULL;
	}

	entry->src_addr = src_addr;
	entry->dest_group = group_addr;
	entry->src_port = src_port; // TODO really like this ? (one AS per sender or per port?)
	entry->dest_port = dest_port;
	entry->iprp_enabled = iprp_enabled;

	return entry;
}