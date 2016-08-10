#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
// TODO include NFQueue

#include "global.h"

// TODO decide error handling
int queue_setup(iprp_queue_t *nfq, int queue_id, nfq_callback *callback) {
	// Setup nfqueue
	nfq->handle = nfq_open();
	if (!nfq->handle) {
		ERR("Unable to open queue handle", IPRP_ERR_NFQUEUE);
	}
	if (nfq_unbind_pf(nfq->handle, AF_INET) < 0) {
		ERR("Unable to unbind IP protocol from queue handle", IPRP_ERR_NFQUEUE);
	}
	if (nfq_bind_pf(nfq->handle, AF_INET) < 0) {
		ERR("Unable to bind IP protocol to queue handle", IPRP_ERR_NFQUEUE);
	}
	nfq->queue = nfq_create_queue(nfq->handle, queue_id, callback, NULL);
	if (!nfq->queue) {
		ERR("Unable to create queue", IPRP_ERR_NFQUEUE);
	}
	if (nfq_set_queue_maxlen(nfq->queue, IPRP_NFQUEUE_MAX_LENGTH) == -1) {
		ERR("Unable to set queue max length", IPRP_ERR_NFQUEUE);
	}
	if (nfq_set_mode(nfq->queue, NFQNL_COPY_PACKET, 0xffff) == -1) { // TODO why 0xffff and not UINT32_MAX
		ERR("Unable to set queue mode", IPRP_ERR_NFQUEUE);
	}
	nfq->fd = nfq_fd(nfq->handle);

	return 0;
}

int get_and_handle(struct nfq_handle *handle, int queue_fd) {
	int bytes;
	char buf[IPRP_PKTBUF_SIZE];

	// Get packet from queue
	if ((bytes = recv(queue_fd, buf, IPRP_PKTBUF_SIZE, 0)) == -1) {
		if (errno == ENOBUFS) {
			return IPRP_ERR_UNKNOWN;
		}
		return IPRP_ERR;
	} else if (bytes == 0) {
		return IPRP_ERR_EMPTY;
	}

	// Handle packet
	if (nfq_handle_packet(handle, buf, bytes) == -1) {
		//ERR("Error while handling packet", err); // TODO why?
	}
	return 0;
}