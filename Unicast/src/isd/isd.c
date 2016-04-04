#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <stdint.h> // Warning: include before netfilter queue
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/netfilter.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <unistd.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <string.h>
#include <stdbool.h>

#include "../../inc/isd.h"
#include "../../inc/global.h"
#include "../../inc/sender.h"

// Thread variables
pthread_t send_thread;
pthread_t cache_thread;

// Global variables
uint16_t reboot_counter;
uint32_t seq_nb = 1;
int sockets[IPRP_MAX_IFACE]; // TODO dynamic management of sockets (or one) and source port transmission

// Peerbase cache
iprp_peerbase_t base;
pthread_mutex_t base_mutex = PTHREAD_MUTEX_INITIALIZER;
bool base_loaded = false; // TODO condition variable

// NFQueue variables
struct nfq_handle *handle;
struct nfq_q_handle *queue;
int queue_fd;

int main(int argc, char const *argv[]) {
	int err;

	// Get arguments
	int queue_id = atoi(argv[1]);
	int receiver_id = atoi(argv[2]);
	DEBUG(IPRP_ISD, "Started");

	// Compute reboot counter
	srand(time(NULL));
	reboot_counter = (uint16_t) rand();

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
	DEBUG(IPRP_ISD, "NFQueue created");

	// Create send sockets
	// TODO isn't it overkill if most are not used?
	// TODO do we really need multiple sockets ? Probably yes
	for (int i = 0; i < IPRP_MAX_IFACE; ++i) {
		if ((sockets[i] = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
			ERR("Unable to create socket", errno);
		}
	}
	DEBUG(IPRP_ISD, "Sockets created");

	// Launch cache routine
	if (err = pthread_create(&cache_thread, NULL, cache_routine, (void*) receiver_id)) {
		ERR("Unable to setup caching thread", err);
	}
	DEBUG(IPRP_ISD, "Caching thread created");

	// Launch send routine
	if (err = pthread_create(&send_thread, NULL, send_routine, NULL)) {
		ERR("Unable to setup caching thread", err);
	}
	DEBUG(IPRP_ISD, "Send thread created");
	
	// TODO shouldn't get here ?

	cleanup();

	return EXIT_SUCCESS;
}

void* send_routine(void* arg) {
	DEBUG(IPRP_ISD_SEND, "In routine");

	// Wait for the cleanup routine to load the peerbase the first time
	while(!base_loaded) {
		sched_yield();
	}
	DEBUG(IPRP_ISD_SEND, "Base loaded");

	// Handle outgoing packets
	int bytes;
	char buf[IPRP_PACKET_BUFFER_SIZE];
	while (true) {
		if ((bytes = recv(queue_fd, buf, IPRP_PACKET_BUFFER_SIZE, 0)) == -1) {
			if (errno == ENOBUFS) {
				// TODO add configuration to see if OK
				LOG(IPRP_ISD_SEND, "Queue is dropping packets");
				continue;
			}
			ERR("Unable to read packet from queue", errno);
		} else if (bytes == 0) {
			// Receiver has performed an orderly shutdown.
			// TODO Can this happen ? What if yes ?
			DEBUG(IPRP_ISD_SEND, "No bytes received");
		}
		DEBUG(IPRP_ISD_SEND, "Received packet");

		// Handle packet
		int err;
		if ((err = nfq_handle_packet(handle, buf, IPRP_PACKET_BUFFER_SIZE)) == -1) {
			//ERR("Error while handling packet", err); // TODO why?
		}
		DEBUG(IPRP_ISD_SEND, "Packet handled");
	}
}

void* cache_routine(void *arg) {
	// Get argument
	int receiver_id = (int) arg;
	DEBUG(IPRP_ISD_CACHE, "In routine");

	// Compute file name
	char base_path[IPRP_PATH_LENGTH];
	snprintf(base_path, IPRP_PATH_LENGTH, "files/base_%x.iprp", receiver_id);

	while(true) {
		iprp_peerbase_t temp;

		// Load peerbase from file
		int err;
		if (err = peerbase_load(base_path, &temp)) {
			ERR("Unable to load peerbase", err);
		}
		DEBUG(IPRP_ISD_CACHE, "Peerbase loaded");

		// Update 
		pthread_mutex_lock(&base_mutex);
		base = temp;
		pthread_mutex_unlock(&base_mutex);
		base_loaded = true;
		DEBUG(IPRP_ISD_CACHE, "Peerbase cached");

		// TODO Update sockets according to loaded peerbase (or not ?). For now not.
		LOG(IPRP_ISD_CACHE, "Peerbase cached");
		sleep(IPRP_ISD_TCACHE);
	}
}

int handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data) {
	// Get header
	struct nfqnl_msg_packet_hdr *nfq_header = nfq_get_msg_packet_hdr (packet); // TODO no error check in IPv6 version
	if (!nfq_header) {
		// TODO just let it go?
		ERR("Unable to retrieve header form received packet", IPRP_ERR_NFQUEUE);
	}
	DEBUG(IPRP_ISD_HANDLE, "Got header");

	// Get packet payload
	unsigned char *buf;
	int bytes;
	if ((bytes = nfq_get_payload(packet, &buf)) == -1) {
		// TODO just let it go ?
		ERR("Unable to retrieve payload from received packet", IPRP_ERR_NFQUEUE);
	}
	DEBUG(IPRP_ISD_HANDLE, "Got payload");

	// Get payload headers
	struct iphdr *ip_header = (struct iphdr *) buf;
	struct udphdr *udp_header = (struct udphdr *) (buf + sizeof(struct iphdr)); // TODO sizeof(uint32_t) * ip_header->ip_hdr_len
	DEBUG(IPRP_ISD_HANDLE, "Got payload headers");

	// Create new packet
	char *new_packet = malloc(bytes - sizeof(struct iphdr) - sizeof(struct udphdr) + sizeof(iprp_header_t));
	if (new_packet == NULL) {
		// TODO handle this better
		ERR("Unable to allocate new packet buffer", errno);
	}
	memcpy(new_packet + sizeof(iprp_header_t), buf + sizeof(struct iphdr) + sizeof(struct udphdr), bytes - sizeof(struct iphdr) - sizeof(struct udphdr));
	DEBUG(IPRP_ISD_HANDLE, "Created new packet");

	// Create IPRP header
	iprp_header_t *header = (iprp_header_t *) new_packet;
	header->version = IPRP_VERSION;
	header->seq_nb = seq_nb;
	header->dest_port = udp_header->dest;

	// Create SNSID
	// TODO compute snsid from peer base?
	for (int i = 0; i < 4; ++i) {
		memcpy(&header->snsid[i * 4], &ip_header->saddr, 4);
	}
	memcpy(&header->snsid[16], &udp_header->source, 2);
	memcpy(&header->snsid[18], &reboot_counter, 2);
	DEBUG(IPRP_ISD_HANDLE, "IPRP header created");

	// Duplicate packets
	int socket_id = 0;
	for (int i = 0; i < IPRP_MAX_INDS; ++i) {
		if (base.paths[i].active) {
			// Send packet on IND i
			header->ind = base.paths[i].iface.ind;
			// TODO compute MAC

			// Compute destination
			struct sockaddr_in recv_addr;
			recv_addr.sin_family = AF_INET;
			recv_addr.sin_port = htons(IPRP_DATA_PORT);
			recv_addr.sin_addr = base.paths[i].dest_addr;
			memset(&recv_addr.sin_zero, 0, sizeof(recv_addr.sin_zero));

			// Send message
			if (sendto(sockets[socket_id],
					new_packet,
					bytes - sizeof(struct iphdr) - sizeof(struct udphdr) + sizeof(iprp_header_t),
					0,
					(struct sockaddr *) &recv_addr,
					sizeof(recv_addr)) == -1) {
				ERR("Unable to send packet", errno);
			}
			DEBUG(IPRP_ISD_HANDLE, "Packet sent");

			socket_id++;
		}
	}
	LOG(IPRP_ISD_HANDLE, "Outgoing packet handled. All duplicate packets sent.");

	// Compute next sequence number
	seq_nb = (seq_nb == UINT32_MAX) ? 1 : seq_nb + 1;
	
	// Drop packet (TODO earlier)
	int err;
	if ((err = nfq_set_verdict(queue, ntohl(nfq_header->packet_id), NF_DROP, bytes, buf)) == -1) {
		ERR("Unable to set verdict", IPRP_ERR_NFQUEUE);
	}
	DEBUG(IPRP_ISD_HANDLE, "Packet dropped from queue");
	return 0;
}

void cleanup() {
	// TODO implement clean ISD shutdown
	nfq_destroy_queue(queue);
	nfq_close(handle);
}