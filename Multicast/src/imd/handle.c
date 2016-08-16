/**\file imd/handle.c
 * Packet handler for the IMD queues
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 */
#define IPRP_FILE IMD_HANDLE

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <linux/ip.h>
#include <linux/udp.h>

#include "imd.h"

extern time_t curr_time;
extern list_t active_senders;

/* Function prototypes */
int handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data);
int ird_handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data);
int global_handle(struct nfq_q_handle *queue, struct nfq_data *packet, bool iprp_message);
iprp_active_sender_t *activesenders_find_entry(const struct in_addr src_addr, const struct in_addr group_addr, const uint16_t src_port, const uint16_t dest_port);
iprp_active_sender_t *activesenders_create_entry(const struct in_addr src_addr, const struct in_addr group_addr, const uint16_t src_port, const uint16_t dest_port, const bool iprp_enabled);

/**
 Sets up and launches the wrapper for the IMD queue

 The IMD queue gets all packets sent to any monitored port from non-iPRP sources.
 Those packets can come either from an host to which no iPRP session has been established,
 or a periodical packet to allow newly joining host to the multicast group to extablish their session.
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
int handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data) {
	DEBUG("Handling packet");

	return global_handle(queue, packet, false);
}

/**
 Sets up and launches the wrapper for the IRD-IMD queue

 The IRD-IMD queue transfers packets received by the IRD to update the IMD structures needed.
*/
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
int ird_handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data) {
	DEBUG("Handling packet");

	return global_handle(queue, packet, true);
}

/**
 Handle the message coming from one of the two queues monitored by the IMD

 The handler first creates or updates the active senders entry for the incoming packet.
 It then accepts the packet if it is an iPRP packet, or if no session is established yet.
 Otherwise it rejects the packet (if the packet is a non-iPRP packet sent from an iPRP host).
*/
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