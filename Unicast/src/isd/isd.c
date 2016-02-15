#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h> // Warning: include before netfilter queue
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <unistd.h>

#include "../../inc/isd.h"
#include "../../inc/types.h"
#include "../../inc/sender.h"
#include "../../inc/error.h"
#include "../../inc/log.h"

// TODO find way to not need this
iprp_host_t this;

struct nfq_handle *handle;
struct nfq_q_handle *queue;
int queue_fd;

int sockets[IPRP_MAX_IFACE];
iprp_peerbase_t base;
pthread_mutex_t base_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_t cache_thread;

int main(int argc, char const *argv[]) {
	// Get arguments
	int queue_id = atoi(argv[1]);
	int receiver_id = atoi(argv[2]);

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

	// Create send sockets
	// TODO isn't it overkill if most are not used?
	for (int i = 0; i < IPRP_MAX_IFACE; ++i) {
		if ((sockets[i] = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
			ERR("Unable to create socket", errno);
		}
	}

	// Launch cache routine
	int err;
	if (err = pthread_create(&cache_thread, NULL, cache_routine, (void*) receiver_id)) {
		ERR("Unable to setup caching thread", err);
	}

	// Launch send routine
	send_routine();

	// TODO shouldn't get here ?

	cleanup();

	return EXIT_SUCCESS;
}

void send_routine() {
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

		int err;
		if (err = nfq_handle_packet(handle, buf, IPRP_PACKET_BUFFER_SIZE)) {
			ERR("Error while handling packet", err);
		}
	}
}

void *cache_routine(void *arg) {
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

		// TODO Update sockets according to loaded peerbase (or not ?). For now not.

		sleep(IPRP_ISD_TCACHE);
	}
}

int handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data) {
	// Don't give a damn about message
	// TODO do

	// 1. Get header and payload
	struct nfqnl_msg_packet_hdr *header = nfq_get_msg_packet_hdr (packet); // TODO no error check in IPv6 version
	if (!header) {
		// TODO just let it go?
		ERR("Unable to retrieve header form received packet", IPRP_ERR_NFQUEUE);
	}

	unsigned char *buf;
	int bytes;
	if ((bytes = nfq_get_payload(packet, &buf)) == -1) {
		// TODO just let it go ?
		ERR("Unable to retrieve payload from received packet", IPRP_ERR_NFQUEUE);
	}

	// 2. 
}

void cleanup() {
	nfq_destroy_queue(queue);
	nfq_close(handle);
}