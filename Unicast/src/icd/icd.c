/**\file icd.c
 * Control flow of the Control Daemon
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#define IPRP_ICD

#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>

#include "../../inc/icd.h"
#include "../../inc/global.h"
#include "../../inc/sender.h"
#include "../../inc/receiver.h"

volatile iprp_host_t this; /** Information about the current machine */
int ctl_socket;

pid_t ird_pid;

/**
Control daemon entry point

The main() function's role is to setup the environment for the control,
receiver and sender routines, which are the three main routines of the control
daemon. The function processes the command-line arguments and creates the
control socket used to receive control messages from other hosts (in the
control routine), as well as the pipes used to transmit those messages to the
sender or receiver daemons, acconrding to need. The function then launches the
sender and receiver threads and executes the control routine itself.

\param TBD
\return does not return 
*/
int main(int argc, char const *argv[]) {
	/* Phase 0: manual setup */
	// TODO delete when monitoring daemon is operational

	if (argc != 5) return -1;
	this.nb_ifaces = 2;
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

	/* Phase 1: setup environment */
	srand(time(NULL));

	int err;

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

	/* Phase 2 : Listen on iPRP control port */

	// Open socket
	if ((ctl_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		ERR("Unable to create control socket", errno);
	}
	LOG("[main] Control socket created");

	// Bind socket
	struct sockaddr_in ctl_sa;

	ctl_sa.sin_family = AF_INET;
	ctl_sa.sin_port = htons(IPRP_CTL_PORT);
	ctl_sa.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(ctl_socket, (struct sockaddr *) &ctl_sa, sizeof(ctl_sa)) == -1) {
		ERR("Unable to bind control socket", errno);
	}
	LOG("[main] Control socket bound");

	// Begin to listen to the socket and do the job
	control_routine(recv_pipe[1], send_pipe[1]);

	/* Should not reach this part */

	LOG("[main] Out of control routine. Entering infinite loop");
	while(true);
	return EXIT_FAILURE;
}

/**
Dispatcher for control messages

The control routine listens on the iPRP control port and forwards the received
packets to the sender side if it is a CAP message, or to the receiver side if
it is an ACK message. The routine drops any unrecognized packet.

\param ctl_socket the socket on which control packets are sent
\param recv_pipe_write the writing end of the pipe to the receiver routine
\param send_pipe_write the writing end of the pipe to the sender routine
\return does not return
*/
void control_routine(int recv_pipe_write, int send_pipe_write) {
	LOG("[ctl] In routine");
	// Listen for control messages and forward them accordingly
	while (true) {
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
}

/**
Receiver-side control flow

The receiver routine sets up the auxiliary thread(s) needed at the receiver
side (to send CAP messages). It then listens for ACK messages coming from the
control routine and treats them as expected.

\param recv_pipe_read the reading end of the pipe to the control routine
\return does not return
*/
// TODO bypass to sendcap_routine
void* receiver_routine(void *arg) {
	int recv_pipe_read = (int) arg;

	// Launch receiver and monitoring deamons
	pid_t pid = fork();
	if (pid == -1) {
		ERR("Unable to create receiver deamon", errno);
	} else if (!pid) {
		// Child side
		// Create NFqueue
		int queue_id = get_queue_number();
		char shell[100];
		snprintf(shell, 100, "sudo iptables -t mangle -A PREROUTING -p udp --dport 1001 -j NFQUEUE --queue-num %d", queue_id);
		if (system(shell) == -1) {
			ERR("Unable to create nfqueue for IRD", errno);
		}
		// Launch receiver
		char queue_id_str[16];
		sprintf(queue_id_str, "%d", queue_id);
		if (execl(IPRP_IRD_BINARY_LOC, "ird", queue_id_str, NULL) == -1) {
			ERR("Unable to launch receiver deamon", errno);
		}

		LOG("[icd] IRD created");
	} else {
		// Parent side
		ird_pid = pid;
	}

	int err;

	// Setup receiver logic
	if ((err = receiver_init())) {
		ERR("Unable to initialize active senders list", err);
	}
	LOG("[recv] Active senders list initialized");

	// Setup sendcap routine
	pthread_t sendcap_thread;
	if ((err = pthread_create(&sendcap_thread, NULL, receiver_sendcap_routine, NULL)) != 0) {
		ERR("Unable to setup iPRP_CAP sender thread", err);
	}
	LOG("[recv] Sendcap thread created");

	while(true) {
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
}

/**
Sends CAP messages to the active senders

The sendcap routine first sets up a socket to send messages. It then sends CAP
messages every TCAP seconds to all senders currently present in the list of
active senders.

\param none
\return does not return
*/
void* receiver_sendcap_routine(void* arg) {
	LOG("[sendcap] In routine");
	// Create sender socket
	int sendcap_socket;

	if ((sendcap_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		ERR("Unable to create send CAP socket", errno);
	}
	LOG("[sendcap] socket created");

	while(true) {
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

/**
Sender-side control flow

The sender routine receives CAP messages from the control routine and treats them as expected. It updates the necessary peer bases and creates the sender daemons needed to duplicate the outgoing traffic. Before it begins, it also creates a cleanup thread to remove old entries from the peer base.

\param send_pipe_read the reading end of the pipe to the control routine
\return does not return
*/
// TODO bypass pipe (only 1 type) ??
void* sender_routine(void *arg) {
	LOG("[send] in routine");
	int send_pipe_read = (int) arg;

	// Setup sender logic
	int err;
	if ((err = sender_init())) {
		ERR("Unable to initialize peer-base list", err);
	}
	LOG("[send] Peer base list initialized");

	while(true) {
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
				link = malloc(sizeof(iprp_sender_link_t));

				link->dest_addr = addr;
				link->src_port = msg.src_port;
				link->dest_port = msg.dest_port;
				link->receiver_id = (uint32_t) addr.s_addr;
				link->queue_id = get_queue_number();

				LOG("[send] IND matching successful. Inserting into peer base");
				if (err = peerbase_insert(link, &msg.receiver, matching_inds)) {
					ERR("Unable to insert peerbase", err);
				}
				
				// Create sender deamon
				pid_t pid = fork();
				if (pid == -1) {
					ERR("Unable to create sender deamon", errno);
				} else if (!pid) {
					// Child side
					// Create NFqueue
					char dest_addr[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, &link->dest_addr, dest_addr, INET_ADDRSTRLEN);
					char shell[120];
					snprintf(shell, 120, "iptables -t mangle -A POSTROUTING -p udp -d %s --dport %d --sport %d -j NFQUEUE --queue-num %d", dest_addr, link->dest_port, link->src_port, link->queue_id);
					if (system(shell) == -1) {
						ERR("Unable to create nfqueue for ISD", errno);
					}
					// Launch sender
					char receiver_id[16];
					sprintf(receiver_id, "%d", link->receiver_id);
					char queue_id[16];
					sprintf(queue_id, "%d", link->queue_id);
					if (execl(IPRP_ISD_BINARY_LOC, "isd", queue_id, receiver_id, NULL) == -1) {
						ERR("Unable to launch sender deamon", errno);
					}
					LOG("[icd] ISD created");
				} else {
					link->isd_pid = pid;
				}
			}
		}
	}
}