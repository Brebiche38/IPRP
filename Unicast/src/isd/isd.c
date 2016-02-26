#define IPRP_ISD

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

uint16_t reboot_counter;
uint32_t seq_nb = 1;

struct nfq_handle *handle;
struct nfq_q_handle *queue;
int queue_fd;

int sockets[IPRP_MAX_IFACE];

volatile bool base_loaded = false;
iprp_peerbase_t base;
pthread_mutex_t base_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_t cache_thread;

int main(int argc, char const *argv[]) {
	LOG("[isd] started");
	// Get arguments
	int queue_id = atoi(argv[1]);
	int receiver_id = atoi(argv[2]);

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

	errno = 0;
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

	LOG("[isd] nfqueue created");

	// Create send sockets
	// TODO isn't it overkill if most are not used?
	// TODO do we really need multiple sockets ? Probably yes
	for (int i = 0; i < IPRP_MAX_IFACE; ++i) {
		if ((sockets[i] = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
			ERR("Unable to create socket", errno);
		}
	}

	LOG("[isd] sockets created");

	// Launch cache routine
	int err;
	if (err = pthread_create(&cache_thread, NULL, cache_routine, (void*) receiver_id)) {
		ERR("Unable to setup caching thread", err);
	}
	LOG("[isd] caching thread created");

	while(!base_loaded) {
		sched_yield();
	}
	LOG("[isd] base loaded");

	// Launch send routine
	send_routine();
	
	// TODO shouldn't get here ?

	cleanup();

	return EXIT_SUCCESS;
}

void send_routine() {
	LOG("[isd-send] in routine");

	int bytes;
	char buf[IPRP_PACKET_BUFFER_SIZE];

	while (true) {
		if ((bytes = recv(queue_fd, buf, IPRP_PACKET_BUFFER_SIZE, 0)) == -1) {
			if (errno == ENOBUFS) {
				// TODO add configuration to see if OK
				continue;
			}
			ERR("Unable to read packet from queue", errno);
		} else if (bytes == 0) {
			// Receiver has performed an orderly shutdown.
			// TODO Can this happen ? What if yes ?
		}

		LOG("[isd-send] received packet");

		int err;
		if ((err = nfq_handle_packet(handle, buf, IPRP_PACKET_BUFFER_SIZE)) == -1) {
			ERR("Error while handling packet", err);
		}

		LOG("[isd-send] packet handled");
	}
}

void *cache_routine(void *arg) {
	LOG("[isd-cache] in routine");
	int receiver_id = (int) arg;
	char base_path[IPRP_PATH_LENGTH];
	snprintf(base_path, IPRP_PATH_LENGTH, "files/base_%x.iprp", receiver_id);

	while(true) {
		iprp_peerbase_t temp;

		int err;
		if (err = peerbase_load(base_path, &temp)) {
			ERR("Unable to load peerbase", err);
		}

		// update with locks
		pthread_mutex_lock(&base_mutex);
		base = temp;
		pthread_mutex_unlock(&base_mutex);

		base_loaded = true;

		LOG("[isd-cache] peerbase cached");

		// TODO Update sockets according to loaded peerbase (or not ?). For now not.

		sleep(IPRP_ISD_TCACHE);
	}
}

int handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data) {
	// Don't give a damn about message

	// 1. Get header and payload
	struct nfqnl_msg_packet_hdr *nfq_header = nfq_get_msg_packet_hdr (packet); // TODO no error check in IPv6 version
	if (!nfq_header) {
		// TODO just let it go?
		ERR("Unable to retrieve header form received packet", IPRP_ERR_NFQUEUE);
	}
	LOG("[callback] got header");

	unsigned char *buf;
	int bytes;
	if ((bytes = nfq_get_payload(packet, &buf)) == -1) {
		// TODO just let it go ?
		ERR("Unable to retrieve payload from received packet", IPRP_ERR_NFQUEUE);
	}
	LOG("[callback] got payload");

	// Get payload headers
	struct iphdr *ip_header = (struct iphdr *) buf;
	struct udphdr *udp_header = (struct udphdr *) (buf + sizeof(struct iphdr)); // TODO sizeof(uint32_t) * ip_header->ip_hdr_len

	// 2. Create iPRP header and new packet
	char *new_packet = malloc(bytes - sizeof(struct iphdr) - sizeof(struct udphdr) + sizeof(iprp_header_t));
	if (new_packet == NULL) {
		// TODO handle this better
		ERR("Unable to allocate new packet buffer", errno);
	}

	iprp_header_t *header = (iprp_header_t *) new_packet;

	header->version = IPRP_VERSION;

	// TODO compute snsid from peer base ?
	for (int i = 0; i < 4; ++i) {
		memcpy(&header->snsid[i * 4], &ip_header->saddr, 4);
	}
	memcpy(&header->snsid[16], &udp_header->source, 2);
	memcpy(&header->snsid[18], &reboot_counter, 2);
	
	header->seq_nb = seq_nb;
	header->dest_port = udp_header->dest;

	LOG("[callback] got to interesting part");
	// Duplicate packets
	int socket_id = 0;
	for (int i = 0; i < IPRP_MAX_INDS; ++i) {
		if (base.paths[i].active) {
			header->ind = base.paths[i].iface.ind;
			// TODO compute MAC

			memcpy(new_packet + sizeof(iprp_header_t), buf + sizeof(struct iphdr) + sizeof(struct udphdr), bytes - sizeof(struct iphdr) - sizeof(struct udphdr));

			struct sockaddr_in recv_addr;
			recv_addr.sin_family = AF_INET;
			recv_addr.sin_port = htons(IPRP_DATA_PORT);
			recv_addr.sin_addr = base.paths[i].dest_addr;
			memset(&recv_addr.sin_zero, 0, sizeof(recv_addr.sin_zero));

			if (sendto(sockets[socket_id], new_packet, bytes - sizeof(struct iphdr) - sizeof(struct udphdr) + sizeof(iprp_header_t), 0, (struct sockaddr *) &recv_addr, sizeof(recv_addr)) == -1) {
				ERR("Unable to send packet", errno);
			}

			socket_id++;
		}
	}
	// Increase seq nb
	seq_nb = (seq_nb == UINT32_MAX) ? 1 : seq_nb + 1;
	LOG("[callback] reached end");
	int err;
	if ((err = nfq_set_verdict(queue, ntohl(nfq_header->packet_id), NF_DROP, bytes, buf)) == -1) { // TODO NF_DROP
		ERR("Unable to set verdict", IPRP_ERR_NFQUEUE);
	}
	LOG("[callback] really reached end");
	return 0;
}

void cleanup() {
	nfq_destroy_queue(queue);
	nfq_close(handle);
}