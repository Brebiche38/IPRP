#include <stdint.h>
#include <pthread.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/netfilter.h>
#include <libnetfilter_queue/libnetfilter_queue.h>

#include "../../imd.h"
#include "../../global.h"
#include "../../receiver.h"

pthread_t cleanup_thread;

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

}

int handle_packet(struct nfq_q_handle *queue, struct nfgenmsg *message, struct nfq_data *packet, void *data) {
	// Check port, launch corresponding stuff
}