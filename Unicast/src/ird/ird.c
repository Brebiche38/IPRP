#include <stdint.h> // Warning: include before netfilter queue
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/netfilter.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <unistd.h>

#include "../../inc/ird.h"
#include "../../inc/global.h"
#include "../../inc/receiver.h"

struct nfq_handle *handle;
struct nfq_q_handle *queue;
int queue_fd;

list_t receiver_links;

int imd_queue_id;

pthread_t cleanup_thread;

int main(int argc, char const *argv[]) {
	LOG("[ird] started");
	// Get arguments
	int queue_id = atoi(argv[1]);
	imd_queue_id = atoi(argv[2]);

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

	LOG("[ird] nfqueue created");

	// Launch management routine
	int err;
	if ((err = pthread_create(&cleanup_thread, NULL, cleanup_routine, NULL))) {
		ERR("Unable to setup cleanup thread", err);
	}
	LOG("[ird] cleanup thread created");

	// Launch receiving routine
	receive_routine();

	LOG("[ird] out of receive routine. This should not happen");
	return EXIT_FAILURE;
}

void receive_routine() {
	LOG("[ird-receive] in routine");

	list_init(&receiver_links);
	LOG("[ird-receive] Receiver links list initialized");

	int bytes;
	char buf[IPRP_PACKET_BUFFER_SIZE];

	while (true) {
		if ((bytes = recv(queue_fd, buf, IPRP_PACKET_BUFFER_SIZE, 0)) == -1) {
			if (errno == ENOBUFS) {
				// TODO add configuration to see if OK
				LOG("[ird-receive] too many messages in queue");
				continue;
			}
			ERR("Unable to read packet from queue", errno);
		} else if (bytes == 0) {
			// Receiver has performed an orderly shutdown.
			// TODO Can this happen ? What if yes ?
			LOG("[ird-receive] No bytes received");
		}

		LOG("[ird-receive] received packet");

		int err;
		if (err = nfq_handle_packet(handle, buf, IPRP_PACKET_BUFFER_SIZE)) {
			//ERR("Error while handling packet", err); // TODO why?
		}

		LOG("[ird-receive] packet handled");
	}

	LOG("[ird-receive] out of loop");
}

// TODO complete
void *cleanup_routine(void* arg) {
	LOG("[ird-cleanup] in routine");

	while (true) {
		LOG("[ird-cleanup] triggered");
		sleep(IPRP_IRD_TCACHE);
	}
	return NULL;
}

int handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data) {
	// Don't give a damn about message
	LOG("[ird-handle] Handling packet");

	// 1. Get header and payload
	struct nfqnl_msg_packet_hdr *nfq_header = nfq_get_msg_packet_hdr (packet); // TODO no error check in IPv6 version
	if (!nfq_header) {
		// TODO just let it go?
		ERR("Unable to retrieve header from received packet", IPRP_ERR_NFQUEUE);
	}
	LOG("[ird-handle] Got header");

	unsigned char *buf;
	int bytes;
	if ((bytes = nfq_get_payload(packet, &buf)) == -1) {
		// TODO just let it go ?
		ERR("Unable to retrieve payload from received packet", IPRP_ERR_NFQUEUE);
	}
	LOG("[ird-handle] Got payload");

	// Get payload headers
	struct iphdr *ip_header = (struct iphdr *) buf; // TODO assert IP header length = 20 bytes
	struct udphdr *udp_header = (struct udphdr *) (buf + sizeof(struct iphdr)); // TODO sizeof(uint32_t) * ip_header->ip_hdr_len
	iprp_header_t *iprp_header = (iprp_header_t *) (buf + sizeof(struct iphdr) + sizeof(struct udphdr));

	if (iprp_header->version != IPRP_VERSION) {
		// TODO leave him be
		ERR("Received packet with wrong version", 0);
	}

	LOG("[ird-handle] Got to interesting part");

	// Phase 1 : query receiver links
	iprp_receiver_link_t *packet_link = NULL;

	list_elem_t *iterator = receiver_links.head;

	while(iterator != NULL) {
		iprp_receiver_link_t *link = (iprp_receiver_link_t *) iterator->elem;
		bool same = true;

		for (int i = 0; i < 20; ++i) {
			if (link->snsid[i] != iprp_header->snsid[i]) {
				same = false;
				break;
			}
		}
		if (same) {
			packet_link = link;
			break;
		}
		iterator = iterator->next;
	}

	LOG("[ird-handle] Got the packet link");

	bool fresh;

	if (!packet_link) { // TODO auxiliary function
		LOG("[ird-handle] Creating receiver link");
		// Create receiver link TODO check null
		packet_link = malloc(sizeof(iprp_receiver_link_t));

		memcpy(&packet_link->src_addr, &iprp_header->snsid, sizeof(struct in_addr));
		memcpy(&packet_link->src_port, &iprp_header->snsid[16], sizeof(uint16_t));
		packet_link->src_port = ntohs(packet_link->src_port);
		memcpy(&packet_link->snsid, &iprp_header->snsid, 20);

		for (int i = 0; i < IPRP_DD_MAX_LOST_PACKETS; ++i) {
			packet_link->list_sn[i] = 0;
		}
		packet_link->high_sn = iprp_header->seq_nb;
		packet_link->last_seen = time(NULL);

		list_append(&receiver_links, packet_link);

		fresh = true;

		LOG("[ird-handle] Receiver link created");
	} else {
		LOG("[ird-handle] Known receiver link");
		packet_link->last_seen = time(NULL);
		fresh = is_fresh_packet(iprp_header, packet_link);
	}

	if (fresh) {
		LOG("[ird-handle] Fresh packet received");
		// Transfer packet to application
		// Overwrite IPRP header with payload

		uint16_t dest_port = ntohs(iprp_header->dest_port);

		memmove(iprp_header,
			((char*) iprp_header) + sizeof(iprp_header_t),
			bytes - sizeof(struct iphdr) - sizeof(struct udphdr) - sizeof(iprp_header_t));
		
		// Compute IP ckecksum
		ip_header->tot_len = htons(bytes - sizeof(iprp_header_t));
		ip_header->check = 0;
		ip_header->check = ip_checksum(ip_header, sizeof(struct iphdr));

		udp_header->dest = htons(dest_port);
		udp_header->len = htons(bytes - sizeof(struct iphdr) - sizeof(iprp_header_t));

		// Compute UDP checksum (not mandatory)
		udp_header->check = 0;
		//udp_header->check = udp_checksum((uint16_t *) udp_header, bytes - sizeof(struct iphdr), ip_header->saddr, ip_header->daddr);

		LOG("[ird-handle] Packet ready, setting verdict...");

		// Forward packet to application
		int verdict = NF_QUEUE | (imd_queue_id << 16)

		if (nfq_set_verdict(queue, ntohl(nfq_header->packet_id), verdict, bytes - sizeof(iprp_header_t), buf) == -1) {
			ERR("Unable to set verdict to NF_ACCEPT", IPRP_ERR_NFQUEUE);
		}
	} else {
		LOG("[ird-handle] Packet is not fresh, discarding");
		// Drop packet
		if (nfq_set_verdict(queue, ntohl(nfq_header->packet_id), NF_DROP, bytes, buf) == -1) {
			ERR("Unable to set verdict to NF_DROP", IPRP_ERR_NFQUEUE);
		}
	}

	return 0;
}
