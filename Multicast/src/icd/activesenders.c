/**\file icd/activesenders.c
 * Active sender handler for the ICD side
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 */
#define IPRP_FILE ICD_AS

#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <sys/socket.h>

#include "icd.h"
#include "activesenders.h"

extern time_t curr_time;

/* Function prototypes */
int get_active_senders(iprp_active_sender_t **senders);
void send_cap(iprp_active_sender_t *sender, int socket);
int backoff();

/**
 Sends CAP messages to the active senders
 
 The active senders routine initializes the active sender file (to avoid reading an unexisting file).
 It then sets up a socket to send the CAP messages.
 Then periodically after a backoff period, it sends CAP messages to all active senders.
*/
void* as_routine(void* arg) {
	DEBUG("In routine");
	srand(curr_time);

	// Initialize active sender list
	activesenders_store(IPRP_AS_FILE, 0, NULL);
	DEBUG("Active senders list initialized");

	// Create sender socket
	int sendcap_socket;
	if ((sendcap_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		ERR("Unable to create send CAP socket", errno);
	}
	DEBUG("Socket created");

	while(true) {
		// Get active senders
		iprp_active_sender_t* senders;
		int count = get_active_senders(&senders);
		DEBUG("Active senders retrieved");

		// Send CAP messages
		for (int i = 0; i < count; ++i) {
			send_cap(&senders[i], sendcap_socket);
			DEBUG("CAP sent");
		}
		LOG("All CAPs sent");

		sleep(IPRP_TCAP + backoff());
	}
}

/**
 Reads active senders from file
*/
int get_active_senders(iprp_active_sender_t **senders) {
	int count;
	int err;
	if ((err = activesenders_load(IPRP_AS_FILE, &count, senders))) {
		ERR("Unable to get active senders", err);
	}

	return count;
}

/**
 Creates and sends a CAP message to a given sender
*/
void send_cap(iprp_active_sender_t *sender, int socket) {
	// Create message
	iprp_ctlmsg_t msg;
	msg.secret = IPRP_CTLMSG_SECRET;
	msg.msg_type = IPRP_CAP;
	msg.message.cap_message.iprp_version = IPRP_VERSION;
	msg.message.cap_message.group_addr = sender->dest_group;
	msg.message.cap_message.src_addr = sender->src_addr;
	msg.message.cap_message.inds = 0xf; // TODO change
	msg.message.cap_message.src_port = sender->src_port;
	msg.message.cap_message.dest_port = sender->dest_port;

	// Send message
	struct sockaddr_in addr;
	sockaddr_fill(&addr, sender->src_addr, IPRP_CTL_PORT);
	sendto(socket, (void*) &msg, sizeof(msg), 0, (struct sockaddr*) &addr, sizeof(addr));
}

// TODO really the good algorithm?
/**
 Computes the time to wait before sending the next round of CAPs
*/
int backoff() {
	srand(curr_time);
	double random = 0.0;
	double x = 2;
	while(random == 0.0 && x > 1) {
		random = rand();
		x = -log(random/(RAND_MAX)) / IPRP_BACKOFF_LAMBDA;
	}
	return (1-x)*IPRP_BACKOFF_D;
}