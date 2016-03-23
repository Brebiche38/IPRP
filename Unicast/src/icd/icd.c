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
#include <stdbool.h>
#include <string.h>

#include "../../inc/icd.h"
#include "../../inc/global.h"
#include "../../inc/sender.h"
#include "../../inc/receiver.h"

/* Thread descriptors */
pthread_t control_thread;
pthread_t receiver_thread;
pthread_t sender_thread;
pthread_t sendcap_thread;
pthread_t ports_thread;

/* Global variables */
iprp_host_t this; /** Information about the current machine */

list_t monitored_ports;

pid_t ird_pid; /** PID of the receiver deamon */
pid_t imd_pid;

bool receiver_active = false;
int ird_queue_num;
int imd_queue_num;

int control_socket; /** Socket used to send control messages */
int receiver_pipe[2]; /** Pipe used to transmit messages to the receiver side */
int sender_pipe[2]; /** Pipe used to transmit messages to the sender side */


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
	/* Phase 0: Manual setup (creation of this) */
	if (argc != 3) return -1;
	// Create this
	this.ifaces[0].ind = 0x1;
	inet_pton(AF_INET, argv[1], &this.ifaces[0].addr);
	this.ifaces[1].ind = 0x2;
	inet_pton(AF_INET, argv[2], &this.ifaces[1].addr);
	this.nb_ifaces = 2;

	/* Phase 1: Setup */
	int err;

	// Seed random generator
	srand(time(NULL)); // TODO thread-safe

	// Setup receiver pipe
	if ((err = pipe(receiver_pipe))) { // TODO check that PIPE_BUF is big enough
		ERR("Unable to setup receiver pipe", err);
	}
	LOG("[main] Receiver pipe setup");

	// Setup receiver thread
	if ((err = pthread_create(&receiver_thread, NULL, receiver_routine, NULL))) { // TODO bypass to receiver_sendcap
		ERR("Unable to setup receiver thread", err);
	}
	LOG("[main] Receiver thread setup");

	// Setup sender pipe
	if ((err = pipe(sender_pipe))) { // TODO check that PIPE_BUF is big enough
		ERR("Unable to setup sender pipe", err);
	}
	LOG("[main] Send pipe setup");

	// Setup sender thread
	if ((err = pthread_create(&sender_thread, NULL, sender_routine, NULL))) {
		ERR("Unable to setup sender thread", err);
	}
	LOG("[main] Sender thread setup");

	// Open control socket
	if ((control_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		ERR("Unable to create control socket", errno);
	}
	LOG("[main] Control socket created");

	// Bind control socket to local control port
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(IPRP_CTL_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(control_socket, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
		ERR("Unable to bind control socket", errno);
	}
	LOG("[main] Control socket bound");

	// Setup control thread
	if ((err = pthread_create(&control_thread, NULL, control_routine, NULL))) {
		ERR("Unable to setup control thread", err);
	}
	LOG("[main] Control thread setup");

	// Join on other threads (not expected to happen)
	void* return_value;
	if ((err = pthread_join(control_thread, &return_value))) {
		ERR("Unable to join on control thread", err);
	}
	ERR("Control thread unexpectedly finished execution", (int) return_value);
	if ((err = pthread_join(receiver_thread, &return_value))) {
		ERR("Unable to join on control thread", err);
	}
	ERR("Receiver thread unexpectedly finished execution", (int) return_value);
	if ((err = pthread_join(sender_thread, &return_value))) {
		ERR("Unable to join on control thread", err);
	}
	ERR("Sender thread unexpectedly finished execution", (int) return_value);
	if ((err = pthread_join(sendcap_thread, &return_value))) {
		ERR("Unable to join on control thread", err);
	}
	ERR("Receiver sendcap thread unexpectedly finished execution", (int) return_value);

	/* Should not reach this part */
	LOG("[main] Last man standing at the end of the apocalypse");
	return EXIT_FAILURE;
}

/**
Dispatcher for control messages

The control routine listens on the iPRP control port and forwards the received
packets to the sender side if it is a CAP message, or to the receiver side if
it is an ACK message. The routine drops any unrecognized packet.

\param none
\return does not return
*/
void* control_routine(void *arg) {
	LOG("[ctl] In routine");
	// Listen for control messages and forward them accordingly
	while (true) {
		struct sockaddr_in source_sa;
		iprp_ctlmsg_t msg;
		// Receive message
		int bytes;
		socklen_t source_sa_len = sizeof(source_sa);

		if ((bytes = recvfrom(control_socket, &msg, sizeof(iprp_ctlmsg_t), 0, (struct sockaddr *) &source_sa, &source_sa_len)) == -1) {
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
				if ((bytes = write(sender_pipe[1], &msg.message.cap_message, sizeof(iprp_capmsg_t))) != sizeof(iprp_capmsg_t)) {
					ERR("Unable to write CAP message into sender pipe", bytes);
				}
				if ((bytes = write(sender_pipe[1], &source_sa.sin_addr, sizeof(struct in_addr))) != sizeof(struct in_addr)) {
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

\param none
\return does not return
*/
// TODO bypass to sendcap_routine
void* receiver_routine(void *arg) {
	int err;

	activesenders_store(IPRP_ACTIVESENDERS_FILE, 0, NULL);

	// Setup port update routine
	if ((err = pthread_create(&ports_thread, NULL, receiver_ports_routine, NULL)) != 0) {
		ERR("Unable to setup port manager thread", err);
	}
	LOG("[recv] Sendcap thread created");	

	// Setup sendcap routine
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

void* receiver_ports_routine(void* arg) {
	LOG("[ports] In routine");

	list_init(&monitored_ports);

	// Assign IMD/ICD queue numbers
	do {
		ird_queue_num = rand() % 65535;
		imd_queue_num = rand() % 65535;
	} while (ird_queue_num == imd_queue_num);

	while(true) {
		// Get list of ports
		uint16_t *new_ports;

		size_t num_ports = get_monitored_ports(&new_ports);
		if (num_ports < 0) {
			ERR("Unable to retrieve list of monitored ports", num_ports);
		}

		// Close expired ports
		list_elem_t *iterator = monitored_ports.head;
		while(iterator != NULL) {
			uint16_t port = (uint16_t) iterator->elem;
			bool found = false;
			for (int i = 0; i < num_ports; ++i) {
				if (port == new_ports[i]) {
					found = true;
					break;
				}
			}

			if (!found) {
				// Delete list item, iptables rule
				list_elem_t *next = iterator->next;
				list_delete(&monitored_ports, iterator);
				iterator = next;

				// Delete iptables rule
				char buf[100];
				snprintf(buf, 100, "sudo iptables -t mangle -D PREROUTING -p udp --dport %d -j NFQUEUE --queue-num %d", port, imd_queue_num);
				system(buf);
			} else {
				iterator = iterator->next;
			}
		}

		// Open new ports
		for (int i = 0; i < num_ports; ++i) {
			bool found = false;
			list_elem_t *iterator = monitored_ports.head;
			while(iterator != NULL) {
				uint16_t port = (uint16_t) iterator->elem;
				if (port == new_ports[i]) {
					found = true;
					break;
				}
			}
			if (!found) {
				// Create list item
				list_append(&monitored_ports, (void*) new_ports[i]);

				// Create iptables rule
				char buf[100];
				snprintf(buf, 100, "sudo iptables -t mangle -A PREROUTING -p udp --dport %d -j NFQUEUE --queue-num %d", new_ports[i], imd_queue_num);
				system(buf);				
			}
		}

		// Create or delete IRD/IMD
		if (receiver_active && (list_size(&monitored_ports) == 0)) {
			// TODO Delete both

			receiver_active = false;
		} else if (!receiver_active && list_size(&monitored_ports) > 0) {
			pid_t pid;
			// Launch receiver deamon
			pid = fork();
			if (pid == -1) {
				ERR("Unable to create receiver deamon", errno);
			} else if (!pid) {
				// Child side
				// Create NFqueue
				char shell[100];
				snprintf(shell, 100, "sudo iptables -t mangle -A PREROUTING -p udp --dport 1001 -j NFQUEUE --queue-num %d", ird_queue_num);
				if (system(shell) == -1) {
					ERR("Unable to create nfqueue for IRD", errno);
				}
				// Launch receiver
				char ird_queue_id_str[16];
				sprintf(ird_queue_id_str, "%d", ird_queue_num);
				char imd_queue_id_str[16];
				sprintf(imd_queue_id_str, "%d", imd_queue_num);
				if (execl(IPRP_IRD_BINARY_LOC, "ird", ird_queue_id_str, imd_queue_id_str, NULL) == -1) {
					ERR("Unable to launch receiver deamon", errno);
				}

				LOG("[icd] IRD created");
			} else {
				// Parent side
				ird_pid = pid;
			}

			// Launch monitoring deamon
			pid = fork();
			if (pid == -1) {
				ERR("Unable to create monitoring deamon", errno);
			} else if (!pid) {
				// Child side
				// Launch receiver
				char imd_queue_id_str[16];
				sprintf(imd_queue_id_str, "%d", imd_queue_num);
				if (execl(IPRP_IMD_BINARY_LOC, "imd", imd_queue_id_str, NULL) == -1) {
					ERR("Unable to launch monitoring deamon", errno);
				}

				LOG("[icd] IMD created");
			} else {
				// Parent side
				imd_pid = pid;
			}

			receiver_active = true;
		}

		sleep(IPRP_TPORT);
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
		iprp_active_sender_t* senders;
		int count = get_active_senders(&senders);
		printf("%d\n", count);
		LOG("[sendcap] Active senders retrieved");
		for (int i = 0; i < count; ++i) {
			send_cap(&senders[i].src_addr, sendcap_socket);
			LOG("Send CAP");
		}
		LOG("[sendcap] All CAPs sent. Sleeping...");
		sleep(IPRP_TCAP);
	}
}

/**
Sender-side control flow

The sender routine receives CAP messages from the control routine and treats them as expected. It updates the necessary peer bases and creates the sender daemons needed to duplicate the outgoing traffic. Before it begins, it also creates a cleanup thread to remove old entries from the peer base.

\param none
\return does not return
*/
void* sender_routine(void *arg) {
	LOG("[send] in routine");

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
		if ((bytes = read(sender_pipe[0], &msg, sizeof(iprp_capmsg_t))) != sizeof(iprp_capmsg_t)) {
			ERR("Error while reading from sender pipe", bytes);
		}
		if ((bytes = read(sender_pipe[0], &addr, sizeof(struct in_addr))) != sizeof(struct in_addr)) {
			ERR("Error while reading from sender pipe", bytes);
		}
		LOG("[send] Received message from pipe");

		// Act on received cap

		// 1. Query peer base for source
		iprp_sender_link_t *link = peerbase_query(&addr, msg.src_port, msg.dest_port);

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
			LOG("[send] end loop");
		}
	}
}