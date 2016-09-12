/**\file ird/handle.c
 * Packet handler for the IRD queues
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 */
#define IPRP_FILE IRD_HANDLE

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <pthread.h>

#include "ird.h"

extern time_t curr_time;
extern int imd_queue_id;

/* State information about peers */
list_t receiver_links;
pthread_t cleanup_thread;
void* cleanup_routine(void* arg);

/* Function prototypes */
int handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data);
uint16_t ip_checksum(struct iphdr *header, size_t len);
uint16_t udp_checksum(uint16_t *packet, size_t len, uint32_t src_addr, uint32_t dest_addr);
char *create_new_packet(struct iphdr *ip_header, struct udphdr *udp_header, iprp_header_t *iprp_header, char *payload, size_t payload_size, struct in_addr src_addr);
iprp_receiver_link_t *receiver_link_get(iprp_header_t *header);
iprp_receiver_link_t *receiver_link_create(iprp_header_t *header);
bool is_fresh_packet(iprp_header_t *packet, iprp_receiver_link_t *link);

/**
 Initializes the queue and dispatches the packets to the handle function
*/
void* handle_routine(void* arg) {
	intptr_t queue_id = (intptr_t) arg;
	DEBUG("In routine");

	// Setup NFQueue
	iprp_queue_t nfq;
	queue_setup(&nfq, queue_id, handle_packet);
	DEBUG("NFQueue setup");

	// Initialize link list
	list_init(&receiver_links);
	DEBUG("Receiver links list initialized");

	// Launch cleanup routine
	int err;
	if ((err = pthread_create(&cleanup_thread, NULL, cleanup_routine, NULL))) {
		ERR("Unable to setup cleanup thread", err);
	}
	DEBUG("Cleanup thread created");

	// Handle outgoing packets
	while (true) {
		// Get packet
		int err = get_and_handle(nfq.handle, nfq.fd);
		if (err) {
			if (err == IPRP_ERR) {
				ERR("Unable to retrieve packet from IRD queue", errno);
			}
			DEBUG("Error %d while handling packet", err);
		}
		DEBUG("Packet handled");
	}
}

/**
 Handles a packet received on the IRD queue

 The handler first creates or updates the receiver link structure for the sender of the packet.
 It then applies the duplicate-discard algorithm to decide whether to keep the packet.
 If the packet is fresh, the handler modifies it as needed and forwards it to the application.
 Otherwise it drops it.
*/
int handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data) {
	DEBUG("Handling packet");

	// Get payload
	int bytes;
	unsigned char *buf;
	if ((bytes = nfq_get_payload(packet, &buf)) == -1) {
		// TODO just let it go ?
		ERR("Unable to retrieve payload from received packet", IPRP_ERR_NFQUEUE);
	}
	DEBUG("Got payload");

	// Get payload headers
	struct iphdr *ip_header = (struct iphdr *) buf; // TODO assert IP header length = 20 bytes
	struct udphdr *udp_header = (struct udphdr *) (buf + sizeof(struct iphdr)); // TODO sizeof(uint32_t) * ip_header->ip_hdr_len
	iprp_header_t *iprp_header = (iprp_header_t *) (buf + sizeof(struct iphdr) + sizeof(struct udphdr));
	char *payload = buf + sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(iprp_header_t);
	size_t payload_size = bytes - sizeof(struct iphdr) - sizeof(struct udphdr) - sizeof(iprp_header_t);
	DEBUG("Got packet headers");

	// Lock the whole process to avoid concurrent cleanup work
	list_lock(&receiver_links);

	// Find receiver link
	iprp_receiver_link_t *packet_link = receiver_link_get(iprp_header);
	DEBUG("Got the packet link");

	bool fresh;
	if (!packet_link) {
		// Unknown sender, we must create the link
		DEBUG("Unknown sender");

		// Create receiver link
		packet_link = receiver_link_create(iprp_header);
		if (!packet_link) {
			list_unlock(&receiver_links);
			ERR("Unable to create receiver link", errno);
		}
		DEBUG("Receiver link created");

		// Add to link list
		list_append(&receiver_links, packet_link);
		DEBUG("Receiver link added to list");

		// As it is the first packet we see from this receiver, it is always fresh
		fresh = true;
	} else {
		// Known sender, we apply the duplicate-discard algorithm
		DEBUG("Known sender");

		// Update the link and decide to keep or drop the packet
		packet_link->last_seen = curr_time;
		fresh = is_fresh_packet(iprp_header, packet_link);
	}

	// The work on the link list is over now, we can allow cleanup work to resume
	list_unlock(&receiver_links);
	DEBUG("List unlocked");
	
	// Get header
	struct nfqnl_msg_packet_hdr *nfq_header = nfq_get_msg_packet_hdr (packet); // TODO no error check in IPv6 version
	if (!nfq_header) {
		// TODO just let it go?
		ERR("Unable to retrieve header from received packet", IPRP_ERR_NFQUEUE);
	}
	DEBUG("Got header");

	if (fresh) {
		// Fresh packet, tranfer to application
		DEBUG("Fresh packet received");

		char *new_packet = create_new_packet(ip_header, udp_header, iprp_header, payload, payload_size, packet_link->src_addr);
		size_t new_packet_size = payload_size + sizeof(struct iphdr) + sizeof(struct udphdr);
		DEBUG("Packet ready to forward");

		// Forward packet to IMD
		int verdict = NF_QUEUE | (imd_queue_id << 16);
		if (nfq_set_verdict(queue, ntohl(nfq_header->packet_id), verdict, new_packet_size, new_packet) == -1) {
			ERR("Unable to set verdict to NF_QUEUE", IPRP_ERR_NFQUEUE);
		}
		DEBUG("Packet forwarded");

		LOG("Fresh packet forwarded to application");
	} else {
		// Duplicate packet, we drop it
		DEBUG("Duplicate packet received");
		
		// Drop packet
		if (nfq_set_verdict(queue, ntohl(nfq_header->packet_id), NF_DROP, bytes, buf) == -1) {
			ERR("Unable to set verdict to NF_DROP", IPRP_ERR_NFQUEUE);
		}
		DEBUG("Packet dropped");

		LOG("Duplicate packet dropped");
	}

	return 0;
}

/**
 Creates the packet to be forwarded to the application
*/
char *create_new_packet(struct iphdr *ip_header, struct udphdr *udp_header, iprp_header_t *iprp_header, char *payload, size_t payload_size, struct in_addr src_addr) {
	// Modify IP header
	ip_header->saddr = htonl(src_addr); // No need to change destination address in multicast
	ip_header->tot_len = htons(payload_size + sizeof(struct iphdr) + sizeof(struct udphdr));
	ip_header->check = 0;
	ip_header->check = ip_checksum(ip_header, sizeof(struct iphdr));

	// Modify UDP headers
	udp_header->dest = htons(iprp_header->dest_port); // TODO not sure of the order // TODO source port as well ?
	udp_header->len = htons(payload_size + sizeof(struct udphdr));
	udp_header->check = 0;
	//udp_header->check = udp_checksum((uint16_t *) udp_header, bytes - sizeof(struct iphdr), ip_header->saddr, ip_header->daddr); // TODO warning, must be done after iPRP header is overwritten

	// Move payload over IPRP header
	memmove(iprp_header, payload, payload_size);
	
	return (char *) ip_header;
}

/**
 Look for the SNSID of the given IPRP header in the list of links
*/
iprp_receiver_link_t *receiver_link_get(iprp_header_t *header) {
	iprp_receiver_link_t *packet_link = NULL;

	// Look for SNSID in known receiver links
	list_elem_t *iterator = receiver_links.head;
	while(iterator != NULL) {
		// Compare SNSIDs
		iprp_receiver_link_t *link = (iprp_receiver_link_t *) iterator->elem;
		bool same = true;
		for (int i = 0; i < IPRP_SNSID_SIZE; ++i) {
			if (link->snsid[i] != header->snsid[i]) {
				same = false;
				break;
			}
		}

		// Found the link
		if (same) {
			packet_link = link;
			break;
		}

		iterator = iterator->next;
	}

	return packet_link;
}

/**
 Create a receiver link structure with the given IPRP header.
*/
iprp_receiver_link_t *receiver_link_create(iprp_header_t *header) {
	iprp_receiver_link_t *packet_link = malloc(sizeof(iprp_receiver_link_t));
	if (!packet_link) {
		return NULL;
	}

	memcpy(&packet_link->src_addr, &header->snsid, sizeof(struct in_addr));
	memcpy(&packet_link->src_port, &header->snsid[16], sizeof(uint16_t));
	packet_link->src_port = ntohs(packet_link->src_port);
	memcpy(&packet_link->snsid, &header->snsid, 20);

	for (int i = 0; i < IPRP_DD_MAX_LOST_PACKETS; ++i) {
		packet_link->list_sn[i] = 0;
	}
	packet_link->high_sn = header->seq_nb;
	packet_link->last_seen = curr_time;

	return packet_link;
}

/**
 Duplicate-discard algorithm
*/
bool is_fresh_packet(iprp_header_t *packet, iprp_receiver_link_t *link) {
	// TODO resetCtr doesn't make sense...
	if (packet->seq_nb == link->high_sn) {
		// Duplicate packet
		return false;
	} else {
		if (packet->seq_nb > link->high_sn) {
			// Fresh packet out of order
			// We lose space for received packets (we can accept very late packets although more recent ones would be dropped)
			for (int i = link->high_sn + 1; i < packet->seq_nb; ++i) {
				link->list_sn[i % IPRP_DD_MAX_LOST_PACKETS] = i; // TODO does this really work? What about earlier lost packets?
			}
			link->high_sn = packet->seq_nb;
			//*resetCtr = 0; // TODO in original code, modifies last_seen (highTime)
			return 1;
		}
		else
		{
			if (packet->seq_nb < link->high_sn && link->high_sn - packet->seq_nb > IPRP_DD_MAX_LOST_PACKETS) {
				printf("Very Late Packet\n");
				//*resetCtr = *resetCtr + 1;
			}
			/*if(*resetCtr ==MAX_OLD)
			{
				printf("Reboot Detected\n");
				*highSN = 0;
				*resetCtr = 0;
				for(ii = 0; ii < MAX_LOST; ii++)
					listSN[ii] = -1;
				return 1;
			}*/

			if (link->list_sn[packet->seq_nb % IPRP_DD_MAX_LOST_PACKETS] == packet->seq_nb) {
				// The sequence number is in the list, it is a late packet
				//Remove from List
				link->list_sn[packet->seq_nb % IPRP_DD_MAX_LOST_PACKETS] = 0;
				return true;
			} else {
				return false;
			}
		}
	}
}

/**
 Computes the IP checksum from an IP header
*/
uint16_t ip_checksum(struct iphdr *header, size_t len) {
	uint32_t checksum = 0;
	uint16_t *halfwords = (uint16_t *) header;

	for (int i = 0; i < len/2; ++i) {
		checksum += halfwords[i];
		while (checksum >> 16) {
			checksum = (checksum & 0xFFFF) + (checksum >> 16);
		}
	}
	
	while (checksum >> 16) {
		checksum = (checksum & 0xFFFF) + (checksum >> 16);
	}

	return (uint16_t) ~checksum;
}

/**
 Computes the UDP checksum from a UDP header and the given IP pseudo-header
*/
uint16_t udp_checksum(uint16_t *packet, size_t len, uint32_t src_addr, uint32_t dest_addr) {
	uint32_t checksum = 0;

	// Pseudo-header
	checksum += (((uint16_t *) src_addr)[0]);
	checksum += (((uint16_t *) src_addr)[1]);

	checksum += (((uint16_t *) dest_addr)[0]);
	checksum += (((uint16_t *) dest_addr)[1]);

	checksum += htons(IPPROTO_UDP);
	checksum += htons(len);

	while(checksum >> 16) {
		checksum = (checksum & 0xFFFF) + (checksum >> 16);
	}

	// Calculate the sum
	for (int i = 0; i < len/2; ++i) {
		checksum += packet[i];
		while (checksum >> 16) {
			checksum = (checksum & 0xFFFF) + (checksum >> 16);
		}
	}

	if (len % 2 == 1) {
		checksum += *((uint8_t *) packet[len/2]);
		while (checksum >> 16) {
			checksum = (checksum & 0xFFFF) + (checksum >> 16);
		}
	}

	return (uint16_t) ~checksum;
}

/**
 Deletes expired entries from the receiver link structure
*/
void* cleanup_routine(void* arg) {
	DEBUG("In routine");

	while(true) {
		// Delete aged entries
		list_elem_t *iterator = receiver_links.head;
		while(iterator != NULL) {
			// Fine grain lock to avoid blocking packet treatment for a long time
			list_lock(&receiver_links);

			iprp_receiver_link_t *as = (iprp_receiver_link_t*) iterator->elem;
			if (curr_time - as->last_seen > IRD_T_EXP) {
				// Expired sender
				list_elem_t *to_delete = iterator;
				iterator = iterator->next;
				list_delete(&receiver_links, to_delete);
				DEBUG("Aged receiver");
			} else {
				// Good sender
				iterator = iterator->next;
				DEBUG("Fresh receiver");
			}
			list_unlock(&receiver_links);
		}
		DEBUG("Deleted aged entries");

		LOG("Receiver links cleaned up");
		sleep(IRD_T_CLEANUP);
	}
}