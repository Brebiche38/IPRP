#define IPRP_IRD

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

pthread_t cleanup_thread;

int main(int argc, char const *argv[]) {
	LOG("[ird] started");
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
			ERR("Error while handling packet", err);
		}

		LOG("[ird-receive] packet handled");
	}

	LOG("[ird-receive] out of loop");
}

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
		ERR("Unable to retrieve header form received packet", IPRP_ERR_NFQUEUE);
	}
	LOG("[ird-handle] Got header");

	unsigned char *buf;
	int bytes;
	if ((bytes = nfq_get_payload(packet, &buf)) == -1) {
		// TODO just let it go ?
		ERR("Unable to retrieve payload from received packet", IPRP_ERR_NFQUEUE);
	}
	LOG("[ird-handle] Got payload");
	printf("Payload is %d bytes\n", bytes);

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
		if (link->snsid == iprp_header->snsid) {
			packet_link = link;
			break;
		}
		iterator = iterator->next;
	}

	LOG("[ird-handle] Got the packet link");

	bool fresh;

	if (!packet_link) {
		LOG("[ird-handle] Creating receiver link");
		// Create receiver link
		packet_link = malloc(sizeof(iprp_receiver_link_t));

		memcpy(&packet_link->src_addr, &iprp_header->snsid, sizeof(struct in_addr));
		memcpy(&packet_link->src_port, &iprp_header->snsid[16], sizeof(uint16_t));
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

		printf("%p %p %d\n", iprp_header,
			((char*) iprp_header) + sizeof(iprp_header_t),
			bytes - sizeof(struct iphdr) - sizeof(struct udphdr) - sizeof(iprp_header_t));
		/*memmove(iprp_header,
			((char*) iprp_header) + sizeof(iprp_header_t),
			bytes - sizeof(struct iphdr) - sizeof(struct udphdr) - sizeof(iprp_header_t));
*/
		printf("1\n");
		
		// Compute IP ckecksum
		ip_header->check = 0;
		ip_header->check = ip_checksum(ip_header, sizeof(struct iphdr));

		printf("2\n");

		udp_header->dest = iprp_header->dest_port;

		// Compute UDP checksum (not mandatory)
		udp_header->check = 0;
		//udp_header->check = udp_checksum((uint16_t *) udp_header, bytes - sizeof(struct iphdr), ip_header->saddr, ip_header->daddr);

		LOG("[ird-handle] Packet ready, setting verdict...");

		printf("%d\n", ntohs(udp_header->dest));

		// Forward packet to application
		if (nfq_set_verdict(queue, ntohl(nfq_header->packet_id), NF_ACCEPT, bytes, buf) == -1) { // TODO  - sizeof(iprp_header_t)
			ERR("Unable to set verdict to NF_ACCEPT", IPRP_ERR_NFQUEUE);
		}
	} else {
		LOG("[ird-handle] Packet is not fresh, discarding");
		// Drop packet
		if (nfq_set_verdict(queue, ntohl(nfq_header->packet_id), NF_DROP, bytes, buf) == -1) {
			ERR("Unable to set verdict to NF_DROP", IPRP_ERR_NFQUEUE);
		}
	}
}

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

uint16_t udp_checksum(uint16_t *packet, size_t len, uint32_t src_addr, uint32_t dest_addr) {
	// TODO do
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
