/**\file icd.c
 * Control flow of the Control Daemon
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "../../inc/icd.h"
#include "../../inc/types.h"
#include "../../inc/sender.h"
#include "../../inc/receiver.h"
#include "../../inc/config.h"
#include "../../inc/log.h"
#include "../../inc/util.h"

iprp_host_t this;

int main(int argc, char const *argv[]) {
	// Phase 0: manual setup
	if (argc != 5) return -1;
	// Create this
	this.ifaces[0].ind = 0x1;
	inet_pton(AF_INET, argv[1], &this.ifaces[0].addr);
	this.ifaces[1].ind = 0x2;
	inet_pton(AF_INET, argv[2], &this.ifaces[1].addr);
	// Create active senders entry
	iprp_as_entry_t entry;
	entry.host.ifaces[0].ind = 0x1;
	inet_pton(AF_INET, argv[3], &entry.host.ifaces[0].addr);
	entry.host.ifaces[1].ind = 0x2;
	inet_pton(AF_INET, argv[4], &entry.host.ifaces[1].addr);
	entry.last_seen = time(NULL);
	// Store active senders entry

	FILE* as_list = fopen(IPRP_ACTIVESENDERS_FILE, "w");
	if (as_list == NULL) {
		ERR("Unable to create file descriptor for AS list", errno);
	}
	char line[IPRP_ACTIVESENDERS_LINE_LENGTH];
	get_as_entry(line, &entry);
	fputs(line, as_list);
	fflush(as_list);
	if (fclose(as_list) == EOF) {
		ERR("Unable to close fd for as list", errno);
	}

	// Phase 1: setup environment
	int err;

	// Setup active senders list and peer base
	if ((err = active_senders_init())) {
		ERR("Unable to initialize active senders list", err);
	}
	LOG("[main] Active senders list initialized");

	if ((err = peerbase_init())) {
		ERR("Unable to initialize peer-base list", err);
	}
	LOG("[main] Peer base list initialized");

	// Setup receiver side (thread, pipe)
	pthread_t recv_thread;
	int recv_pipe[2];

	// TODO check that PIPE_BUF is big enough
	if ((err = pipe(recv_pipe))) {
		ERR("Unable to setup receiver pipe", err);
	}
	LOG("[main] Receiver pipe setup");

	if ((err = pthread_create(&recv_thread, NULL, receiver_routine, (void*) recv_pipe[0]))) {
		ERR("Unable to setup receiver thread", err);
	}
	LOG("[main] Receiver thread setup");

	// Setup sender side (thread, pipe)
	pthread_t send_thread;
	int send_pipe[2];

	// TODO check that PIPE_BUF is big enough
	if ((err = pipe(send_pipe))) {
		ERR("Unable to setup sender pipe", err);
	}
	LOG("[main] Send pipe setup");

	if ((err = pthread_create(&send_thread, NULL, sender_routine, (void*) send_pipe[0]))) {
		ERR("Unable to setup sender thread", err);
	}
	LOG("[main] Sender thread setup");

	// Setup monitoring daemon
	// TODO

	// Phase 2 : Listen on iPRP control port
	int ctl_socket;

	// Open socket
	if ((ctl_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		ERR("Unable to create control socket", errno);
	}
	LOG("[main] Control socket created");

	// Bind socket
	struct sockaddr_in ctl_sa;

	ctl_sa.sin_family = AF_INET;
	ctl_sa.sin_port = IPRP_CTL_PORT;
	ctl_sa.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(ctl_socket, (struct sockaddr *) &ctl_sa, sizeof(ctl_sa)) == -1) {
		ERR("Unable to bind control socket", errno);
	}
	LOG("[main] Control socket bound");

	// Begin to listen to the socket and do the job
	control_routine(ctl_socket, recv_pipe[1], send_pipe[1]);

	LOG("[main] Out of control routine. Entering infinite loop");

	while(1);

	return EXIT_FAILURE;
}

void* control_routine(int ctl_socket, int recv_pipe_write, int send_pipe_write) {
	LOG("[ctl] In routine");
	// Listen for control messages and forward them accordingly
	while (1) {
		struct sockaddr_in source_sa;
		iprp_ctlmsg_t msg;
		// Receive message
		int bytes;
		socklen_t source_sa_len = sizeof(source_sa);

		if ((bytes = recvfrom(ctl_socket, &msg, sizeof(iprp_ctlmsg_t), 0, (struct sockaddr *) &source_sa, &source_sa_len)) == -1) {
			ERR("Error while receiving on control socket", errno);
		}
		LOG("[ctl] Received message");

		if (bytes == 0 || bytes != sizeof(iprp_ctlmsg_t) || msg.secret != IPRP_CTLMSG_SECRET) {
			// No packet or wrong packet received, just drop it
			break;
		}

		switch (msg.msg_type) {
			case IPRP_CAP:
				LOG("[ctl] Received CAP message");
				// Follow message to sender thread
				if ((bytes = write(send_pipe_write, &msg.message.cap_message, sizeof(iprp_capmsg_t))) != sizeof(iprp_capmsg_t)) {
					ERR("Unable to write CAP message into sender pipe", bytes);
				}
				if ((bytes = write(send_pipe_write, &source_sa.sin_addr, sizeof(struct in_addr))) != sizeof(struct in_addr)) {
					ERR("Unable to write CAP source into sender pipe", bytes);
				}
				break;
			case IPRP_ACK:
				LOG("[ctl] Received ACK message");
				// Drop in unicast
				// Follow message to sender thread
				//if ((bytes = write(recv_pipe_write, &msg.message.ack_message, sizeof(iprp_ackmsg_t))) != sizeof(iprp_ackmsg_t)) {
				//	ERR("Unable to write into sender pipe", bytes);
				//}
				break;
			default:
				break;
		}
		LOG("[ctl] End loop");
	}

	return NULL;
}

// TODO bypass to sendcap_routine
void* receiver_routine(void *arg) {
	LOG("[recv] In routine");
	// Setup sendcap routine
	pthread_t sendcap_thread;
	int err;
	if ((err = pthread_create(&sendcap_thread, NULL, receiver_sendcap_routine, NULL)) != 0) {
		ERR("Unable to setup iPRP_CAP sender thread", err);
	}
	LOG("[recv] Sendcap thread created");

	int recv_pipe_read = (int) arg;
	while(1) {
//		iprp_ackmsg_t msg;
//		int bytes;
//		if ((bytes = read(recv_pipe_read, &msg, sizeof(iprp_ackmsg_t))) != sizeof(iprp_ackmsg_t)) {
//			ERR("Error while reading from receiver pipe", bytes);
//		}
//		printf("%d\n", bytes);
//		LOG("[recv] Received message from pipe");

		// Act on received ack (multicast only)
		// 1. Query for active sender, if not in active senders, establish connection and add to active senders.
		// 2. Mark sender with ACK timestamp
		// TODO What does receiving an ACK when the sender is not active means?
		// Need way to get active sender

		//if (is_active_sender(sender)) ...
	}

	return NULL;
}

void* receiver_sendcap_routine(void* arg) {
	LOG("[sendcap] In routine");
	// Create sender socket
	int sendcap_socket;
	if ((sendcap_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		ERR("Unable to create send CAP socket", errno);
	}
	LOG("[sendcap] socket created");

	while(1) {
		iprp_host_t senders[IPRP_MAX_SENDERS];
		int count = get_active_senders(senders, IPRP_MAX_SENDERS, IPRP_AS_ALL);
		LOG("[sendcap] Active senders retrieved");
		for (int i = 0; i < count; ++i) {
			send_cap(&senders[i], sendcap_socket);
		}
		LOG("[sendcap] All CAPs sent. Sleeping...");
		sleep(IPRP_TCAP);
	}
}

// TODO bypass pipe (only 1 type) ??
void* sender_routine(void *arg) {
	LOG("[send] in routine");
	int send_pipe_read = (int) arg;
	while(1) {
		iprp_capmsg_t msg;
		struct in_addr addr;
		int bytes;
		if ((bytes = read(send_pipe_read, &msg, sizeof(iprp_capmsg_t))) != sizeof(iprp_capmsg_t)) {
			ERR("Error while reading from sender pipe", bytes);
		}
		if ((bytes = read(send_pipe_read, &addr, sizeof(struct in_addr))) != sizeof(struct in_addr)) {
			ERR("Error while reading from sender pipe", bytes);
		}
		LOG("[send] Received message from pipe");

		// Act on received cap

		// 1. Query peer base for source
		iprp_sender_link_t *link = peerbase_query(&msg, &addr);
		if (link) {
			LOG("[send] Found receiver. Updating peer base");
			// 2. If present, update the keep-alive timer
			peerbase_update(link);
		} else {
			LOG("[send] Receiver not found");
			// 3. Else, perform IND matching and create peer-base entry, then send ack message (not in unicast)
			int matching_inds;
			if ((matching_inds = ind_match(&this, &msg.receiver)) != 0) {
				// TODO create link
				link = malloc(sizeof(iprp_sender_link_t));

				link->dest_addr = addr;
				link->src_port = msg.src_port;
				link->dest_port = msg.dest_port;

				LOG("[send] IND matching successful. Inserting into peer base");
				peerbase_insert(link, &msg.receiver, matching_inds);
				// TODO Create sender deamon for new receiver
			}
		}
	}
}