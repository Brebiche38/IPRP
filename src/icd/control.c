/**\file icd/control.c
 * Control message handler
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 */
#define IPRP_FILE ICD_CTL

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "icd.h"
#include "peerbase.h"

extern time_t curr_time;

extern iprp_host_t this;
int control_socket;

#ifdef IPRP_MULTICAST
extern list_t sender_ifaces;
#endif
extern list_t peerbases;

uint16_t reboot_counter = 0;

/* Function prototypes */
void handle_cap(iprp_capmsg_t *msg, struct in_addr *source);
#ifdef IPRP_MULTICAST
void handle_ack(iprp_ackmsg_t *msg);
int send_ack(int socket, iprp_link_t *link);
bool in_sender_ifaces(struct in_addr group_addr, struct in_addr src_addr);
iprp_sender_ifaces_t *create_sender_ifaces(iprp_ackmsg_t *msg);
#endif
int socket_setup();
iprp_icd_base_t *peerbase_query(iprp_capmsg_t *msg);
iprp_icd_base_t *create_base(iprp_capmsg_t *msg, struct in_addr *src, iprp_ind_bitmap_t matching_inds);
void snsid(iprp_link_t *link);
uint16_t get_queue_number();
pid_t isd_startup();

/**
 Dispatch incoming control messages
 
 The control routine first sets up the control socket.
 It then listens on the socket, and forwards the messages it receives to the corresponding handler.
*/
void* control_routine(void *arg) {
	DEBUG("In routine");
	
	control_socket = socket_setup();

	// Listen for control messages and forward them accordingly
	while (true) {
		// Receive message
		struct sockaddr_in source;
		socklen_t source_len = sizeof(source);
		iprp_ctlmsg_t msg;
		int bytes;
		if ((bytes = recvfrom(control_socket, &msg, sizeof(iprp_ctlmsg_t), 0, (struct sockaddr *) &source, &source_len)) == -1) {
			ERR("Error while receiving on control socket", errno);
		}
		if (bytes == 0 || bytes != sizeof(iprp_ctlmsg_t) || msg.secret != IPRP_CTLMSG_SECRET) {
			// No packet or wrong packet received, just drop it
			DEBUG("Received message (dropped)")
			break;
		}
		DEBUG("Received message");

		// Handle message
	#ifndef IPRP_MULTICAST
		handle_cap(&msg.cap_message, &source.sin_addr);
	#else
		switch (msg.msg_type) {
			case IPRP_CAP:
				handle_cap((iprp_capmsg_t *) &msg.message, &source.sin_addr);
				break;
			case IPRP_ACK:
				handle_ack((iprp_ackmsg_t *) &msg.message);
				break;
			default:
				break;
		}
	#endif
		DEBUG("Message handled");
	}
}

/**
 Handle incoming CAP messages

 For each received CAP message, the handler updates or creates the corresponding peerbase.
 It then sends an ACK message in response.
*/
void handle_cap(iprp_capmsg_t *msg, struct in_addr *source) {
	list_lock(&peerbases);
	// Query peer base for source
	iprp_icd_base_t *base = peerbase_query(msg);

	if (base) {
		// The link is present in the peer base
		DEBUG("Receiver found in peer base");
		
		// Update the peerbase
		base->inds |= ind_match(&this, msg->inds);
		base->last_cap = curr_time;
		DEBUG("Peer base updated");
	} else {
		// The link is not present in the peerbase
		DEBUG("Receiver not found in peer base");

		// Perform IND matching
		iprp_ind_bitmap_t matching_inds;
		if ((matching_inds = ind_match(&this, msg->inds)) != 0) {
			DEBUG("IND matching successful");

			// Create sender link structure
			base = create_base(msg, source, matching_inds);

			list_append(&peerbases, base);
			DEBUG("Link inserted into peer base");
		}
	}

#ifdef IPRP_MULTICAST
	if (base) {
		// Send ACK
		int err;
		if ((err = send_ack(control_socket, &base->link))) {
			ERR("Unable to send ACK message", err);
		}
		DEBUG("ACK sent");
	}
#endif

	list_unlock(&peerbases);

	LOG("CAP message handled");
}

#ifdef IPRP_MULTICAST
/**
 Handle incoming ACK messages

 For each received ACK message, the handler extracts the sender interfaces information.
*/
void handle_ack(iprp_ackmsg_t *msg) {
	struct in_addr addr;

	if (!in_sender_ifaces(msg->group_addr, msg->src_addr)) {
		// Create new entry
		iprp_sender_ifaces_t *new_sender = create_sender_ifaces(msg);
		DEBUG("Created sender interfaces entry")

		// Insert link into list
		list_lock(&sender_ifaces);
		list_append(&sender_ifaces, new_sender);
		list_unlock(&sender_ifaces);
		DEBUG("Sender interfaces list updated")
	}

	LOG("ACK message handled");
}

/**
 Sends an ACK message
*/
int send_ack(int socket, iprp_link_t *link) {
	// Create message
	iprp_ctlmsg_t msg;
	msg.secret = IPRP_CTLMSG_SECRET;
	msg.msg_type = IPRP_ACK;
	msg.message.ack_message.host = this;
	msg.message.ack_message.group_addr = link->dest_addr;
	msg.message.ack_message.src_addr = link->src_addr;

	// Send message
	struct sockaddr_in addr;
	sockaddr_fill(&addr, link->dest_addr, IPRP_CTL_PORT);

	if (sendto(control_socket, (void*) &msg, sizeof(msg), 0, (struct sockaddr*) &addr, sizeof(struct sockaddr)) == -1) {
		ERR("Unable to send ACK message", errno);
	}

	return 0;
}

#endif

/**
 Creates and sets up the control socket
*/
int socket_setup() {
	int ctl_socket;

	// Open control socket
	if ((ctl_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		ERR("Unable to create control socket", errno);
	}
	DEBUG("Control socket created");

	// Bind control socket to local control port
	struct sockaddr_in addr;
	struct in_addr any = { INADDR_ANY };
	sockaddr_fill(&addr, any, IPRP_CTL_PORT);
	if (bind(ctl_socket, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
		ERR("Unable to bind control socket", errno);
	}
	DEBUG("Control socket bound");

#ifdef IPRP_MULTICAST
	// Set multicast options
	setsockopt(ctl_socket, IPPROTO_IP, IP_MULTICAST_IF, &this.ifaces[0].addr, sizeof(struct in_addr));
	uint8_t ttl = 255;
	setsockopt(ctl_socket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
	bool loopback = false;
	setsockopt(ctl_socket, IPPROTO_IP, IP_MULTICAST_LOOP, &loopback, sizeof(loopback));
	DEBUG("Control socket multicast options set");
#endif

	return ctl_socket;
}

/**
 Looks up the peerbases for one corresponding to the given CAP message
*/
iprp_icd_base_t *peerbase_query(iprp_capmsg_t *msg) {
	list_elem_t *iterator = peerbases.head;

	while(iterator) {
		iprp_icd_base_t *base = (iprp_icd_base_t *) iterator->elem;
		iprp_link_t *link = &base->link;
		if (link->dest_addr.s_addr == msg->dest_addr.s_addr
			&& link->src_port == msg->src_port
			&& link->dest_port == msg->dest_port) {
			return base;
		}
		iterator = iterator->next;
	}

	return NULL;
}

#ifdef IPRP_MULTICAST
/**
 Looks up the sender interfaces for one corresponding to the given information
*/
bool in_sender_ifaces(struct in_addr group_addr, struct in_addr src_addr) {
	list_elem_t *iterator = sender_ifaces.head;
	while (iterator != NULL) {
		iprp_sender_ifaces_t *elem = (iprp_sender_ifaces_t *) iterator->elem;

		if (elem->group_addr.s_addr == group_addr.s_addr && elem->sender_addr.s_addr == src_addr.s_addr) {
			elem->last_seen = curr_time;
			DEBUG("Sender interfaces found");
			return true;
		}

		iterator = iterator->next;
	}
	DEBUG("Sender interfaces not found")
	return false;
}

/**
 Creates the sender interfaces entry for the given ACK message
*/
iprp_sender_ifaces_t *create_sender_ifaces(iprp_ackmsg_t *msg) {
	iprp_sender_ifaces_t *new_sender = malloc(sizeof(iprp_sender_ifaces_t));
	if (!new_sender) {
		ERR("Unable to allocate new sender interfaces", errno);
	}

	new_sender->sender_addr = msg->src_addr;
	new_sender->group_addr = msg->group_addr;
	new_sender->nb_ifaces = msg->host.nb_ifaces;
	for (int i = 0; i < msg->host.nb_ifaces; ++i) {
		new_sender->ifaces[i] = msg->host.ifaces[i];
		for (int j = 0; j < this.nb_ifaces; ++j) {
			if (msg->host.ifaces[i].ind == this.ifaces[j].ind) {
				new_sender->host_addr[i] = this.ifaces[j].addr;
				break;
			}
		}
	}
	new_sender->last_seen = curr_time;

	return new_sender;
}
#endif

/**
 Creates the peerbase for the given CAP message
*/
iprp_icd_base_t *create_base(iprp_capmsg_t *msg, struct in_addr *src, iprp_ind_bitmap_t matching_inds) {
	iprp_icd_base_t *base = malloc(sizeof(iprp_icd_base_t));
	if (!base) {
		ERR("Unable to allocate ICD base", errno);
	}

	iprp_link_t *link = &base->link;
	link->src_addr = msg->src_addr;
	link->dest_addr = msg->dest_addr;
	link->src_port = msg->src_port;
	link->dest_port = msg->dest_port;
	snsid(link);

	base->inds = matching_inds;
	printf("Bye\n");
#ifndef IPRP_MULTICAST
	printf("Hello\n");
	for (int i = 0; i < msg->receiver.nb_ifaces; ++i) {
		printf("Ind %x, addr %x\n", msg->receiver.ifaces[i].ind, msg->receiver.ifaces[i].addr.s_addr);
		base->dest_addr[msg->receiver.ifaces[i].ind] = msg->receiver.ifaces[i].addr;
	}
#endif
	base->isd_pid = -1;
	base->queue_id = get_queue_number();
	base->last_cap = curr_time;

	return base;
}

/**
 Fills in the SNSID for a given link
*/
void snsid(iprp_link_t *link) {
	for (int i = 0; i < 16/sizeof(link->src_addr); ++i) {
		memcpy(&link->snsid[4*i], &link->src_addr, sizeof(link->src_addr));
	}
	memcpy(&link->snsid[16], &link->src_port, sizeof(link->src_port));
	memcpy(&link->snsid[18], &reboot_counter, sizeof(reboot_counter));
	reboot_counter++;
}

/**
 Returns an unused netfilter queue number
*/
uint16_t get_queue_number() {
	bool ok = false;
	uint16_t num;

	srand(curr_time);
	while(!ok) {
		num = (uint16_t) (rand() % 1024);
		list_elem_t *iterator = peerbases.head;
		while(iterator) {
			iprp_icd_base_t *base = iterator->elem;
			if (base->queue_id == num) {
				ok = false;
				break;
			}
			iterator = iterator->next;
		}

		ok = true;
	}

	return num;
}