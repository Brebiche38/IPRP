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

pthread_t receive_thread;
pthread_t cleanup_thread;

int main(int argc, char const *argv[]) {
	int err;
	
	// Get arguments
	int queue_id = atoi(argv[1]);
	imd_queue_id = atoi(argv[2]);
	DEBUG(IPRP_IRD, "Started");

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
	DEBUG(IPRP_IRD, "NFQueue created");

	// Launch receiving routine
	if ((err = pthread_create(&receive_thread, NULL, receive_routine, NULL))) {
		ERR("Unable to setup receive thread", err);
	}
	DEBUG(IPRP_IRD, "Receive thread created");

	// Launch cleanup routine
	if ((err = pthread_create(&cleanup_thread, NULL, cleanup_routine, NULL))) {
		ERR("Unable to setup cleanup thread", err);
	}
	DEBUG(IPRP_IRD, "Cleanup thread created");

	LOG(IPRP_IRD, "Receiver daemon successfully created");

	// Join on threads (should not happen)
	void* return_value;
	if ((err = pthread_join(receive_thread, &return_value))) {
		ERR("Unable to join on receive thread", err);
	}
	ERR("Cleanup thread unexpectedly finished execution", (int) return_value);
	if ((err = pthread_join(cleanup_thread, &return_value))) {
		ERR("Unable to join on cleanup thread", err);
	}
	ERR("Cleanup thread unexpectedly finished execution", (int) return_value);

	LOG(IPRP_IRD, "Last man standing at the end of the apocalypse");
	return EXIT_FAILURE;
}

void* receive_routine(void* arg) {
	DEBUG(IPRP_IRD_RECV, "In routine");

	// Initialize link list
	list_init(&receiver_links);
	DEBUG(IPRP_IRD_RECV, "Receiver links list initialized");

	// Handle packets from queue
	int bytes;
	char buf[IPRP_PACKET_BUFFER_SIZE];
	while (true) {
		if ((bytes = recv(queue_fd, buf, IPRP_PACKET_BUFFER_SIZE, 0)) == -1) {
			if (errno == ENOBUFS) {
				// The queue is dropping packets
				// TODO add configuration to see if OK
				LOG(IPRP_IRD_RECV, "Too many messages in queue");
				continue;
			}
			ERR("Unable to read packet from queue", errno);
		} else if (bytes == 0) {
			// Receiver has performed an orderly shutdown.
			// TODO Can this happen ? What if yes ?
			DEBUG(IPRP_IRD_RECV, "No bytes received");
		}
		DEBUG(IPRP_IRD_RECV, "Received packet");

		// Handle packet
		int err;
		if (err = nfq_handle_packet(handle, buf, IPRP_PACKET_BUFFER_SIZE)) {
			//ERR("Error while handling packet", err); // TODO why?
		}
		DEBUG(IPRP_IRD_RECV, "Packet handled");
	}
}

void* cleanup_routine(void* arg) {
	DEBUG(IPRP_IRD_CLEANUP, "In routine");

	while(true) {
		// Sample time (for performance)
		time_t curr_time = time(NULL);
		DEBUG(IPRP_IRD_CLEANUP, "Sampled time");

		// Delete aged entries
		int count = 0;
		list_elem_t *iterator = receiver_links.head;
		while(iterator != NULL) {
			// Fine grain lock to avoid blocking packet treatment for a long time
			list_lock(&receiver_links);

			iprp_receiver_link_t *as = (iprp_receiver_link_t*) iterator->elem;
			if (curr_time - as->last_seen > IPRP_IRD_TEXP) {
				// Expired sender
				list_elem_t *to_delete = iterator;
				iterator = iterator->next;
				list_delete(&receiver_links, to_delete);
				DEBUG(IPRP_IRD_CLEANUP, "Aged receiver");
			} else {
				// Good sender
				count++;
				iterator = iterator->next;
				DEBUG(IPRP_IRD_CLEANUP, "Fresh receiver");
			}
			list_unlock(&receiver_links);
		}
		DEBUG(IPRP_IRD_CLEANUP, "Deleted aged entries");

		LOG(IPRP_IRD_CLEANUP, "Receiver links cleaned up");
		sleep(IPRP_IRD_TCLEANUP);
	}
}

int handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data) {
	DEBUG(IPRP_IRD_HANDLE, "Handling packet");

	// Get header
	struct nfqnl_msg_packet_hdr *nfq_header = nfq_get_msg_packet_hdr (packet); // TODO no error check in IPv6 version
	if (!nfq_header) {
		// TODO just let it go?
		ERR("Unable to retrieve header from received packet", IPRP_ERR_NFQUEUE);
	}
	DEBUG(IPRP_IRD_HANDLE, "Got header");

	// Get payload
	int bytes;
	unsigned char *buf;
	if ((bytes = nfq_get_payload(packet, &buf)) == -1) {
		// TODO just let it go ?
		ERR("Unable to retrieve payload from received packet", IPRP_ERR_NFQUEUE);
	}
	DEBUG(IPRP_IRD_HANDLE, "Got payload");

	// Get payload headers
	struct iphdr *ip_header = (struct iphdr *) buf; // TODO assert IP header length = 20 bytes
	struct udphdr *udp_header = (struct udphdr *) (buf + sizeof(struct iphdr)); // TODO sizeof(uint32_t) * ip_header->ip_hdr_len
	iprp_header_t *iprp_header = (iprp_header_t *) (buf + sizeof(struct iphdr) + sizeof(struct udphdr));
	// if (iprp_header->version != IPRP_VERSION) {
	// 	ERR("Received packet with wrong version", 0);
	// }
	DEBUG(IPRP_IRD_HANDLE, "Got packet headers");

	// Lock the whole process to avoid concurrent cleanup work
	list_lock(&receiver_links);
	DEBUG(IPRP_IRD_HANDLE, "List locked");

	// Find receiver link
	iprp_receiver_link_t *packet_link = receiver_link_get(iprp_header);
	DEBUG(IPRP_IRD_HANDLE, "Got the packet link");

	bool fresh;
	if (!packet_link) {
		// Unknown sender, we must create the link
		DEBUG(IPRP_IRD_HANDLE, "Unknown sender");

		// Create receiver link
		packet_link = receiver_link_create(iprp_header);
		if (!packet_link) {
			list_unlock(&receiver_links);
			ERR("Unable to create receiver link", errno);
		}
		DEBUG(IPRP_IRD_HANDLE, "Receiver link created");

		// Add to link list
		list_append(&receiver_links, packet_link);
		DEBUG(IPRP_IRD_HANDLE, "Receiver link added to list");

		// As it is the first packet we see from this receiver, it is always fresh
		fresh = true;
	} else {
		// Known sender, we apply the duplicate-discard algorithm
		DEBUG(IPRP_IRD_HANDLE, "Known sender");

		// Update the link and decide to keep or drop the packet
		packet_link->last_seen = time(NULL);
		fresh = is_fresh_packet(iprp_header, packet_link);
	}

	// The work on the link list is over now, we can allow cleanup work to resume
	list_unlock(&receiver_links);
	DEBUG(IPRP_IRD_HANDLE, "List unlocked");

	if (fresh) {
		// Fresh packet, tranfer to application
		DEBUG(IPRP_IRD_HANDLE, "Fresh packet received");

		// Get destination port from IPRP header
		uint16_t dest_port = ntohs(iprp_header->dest_port);
		uint16_t src_port = ntohs(*((uint16_t*) &iprp_header->snsid[16]));
		struct in_addr src_addr;
		src_addr.s_addr = *((unsigned long*) &iprp_header->snsid[0]);
		struct in_addr dest_addr;
		dest_addr.s_addr = iprp_header->dest_addr.s_addr;
		//inet_aton("10.1.0.3", &dest_addr);

		// Move payload over IPRP header
		memmove(iprp_header,
			((char*) iprp_header) + sizeof(iprp_header_t),
			bytes - sizeof(struct iphdr) - sizeof(struct udphdr) - sizeof(iprp_header_t));

		printf("source port %d\n", src_port);
		printf("dest port %d\n", dest_port);
		// printf("src addr %x\n", ntohl(src_addr.s_addr));
		// printf("src addr %s\n", inet_ntoa(*((struct in_addr *)(&src_addr))));
		// printf("dest addr %s\n", inet_ntoa(dest_addr));
		
		// Compute ckecksums
		ip_header->saddr = src_addr.s_addr;
		ip_header->daddr = dest_addr.s_addr;
		ip_header->tot_len = htons(bytes - sizeof(iprp_header_t));
		printf("IP checksum before: %d", ip_header->check);
		ip_header->check = 0;
		ip_header->check = ip_checksum(ip_header, sizeof(struct iphdr));
		printf("IP checksum after: %d", ip_header->check);

		udp_header->dest = htons(dest_port);
		udp_header->source = htons(src_port);
		udp_header->len = htons(bytes - sizeof(struct iphdr) - sizeof(iprp_header_t));
		printf("len %d %d %d %d\n", bytes, sizeof(struct iphdr), sizeof(iprp_header_t), ntohs(udp_header->len));
		udp_header->check = 0;
		//udp_header->check = udp_checksum((uint16_t *) udp_header, ntohs(udp_header->len), ip_header->saddr, ip_header->daddr);
		DEBUG(IPRP_IRD_HANDLE, "Packet ready to forward");

		// Forward packet to IMD
		int verdict = NF_QUEUE | (imd_queue_id << 16);
		//int verdict = NF_ACCEPT;
		if (nfq_set_verdict(queue, ntohl(nfq_header->packet_id), verdict, bytes - sizeof(iprp_header_t), buf) == -1) {
			ERR("Unable to set verdict to NF_QUEUE", IPRP_ERR_NFQUEUE);
		}
		DEBUG(IPRP_IRD_HANDLE, "Packet forwarded");

		LOG(IPRP_IRD_HANDLE, "Fresh packet forwarded to application");
	} else {
		// Duplicate packet, we drop it
		DEBUG(IPRP_IRD_HANDLE, "Duplicate packet received");
		
		// Drop packet
		if (nfq_set_verdict(queue, ntohl(nfq_header->packet_id), NF_DROP, bytes, buf) == -1) {
			ERR("Unable to set verdict to NF_DROP", IPRP_ERR_NFQUEUE);
		}
		DEBUG(IPRP_IRD_HANDLE, "Packet dropped");

		LOG(IPRP_IRD_HANDLE, "Duplicate packet dropped");
	}

	return 0;
}
