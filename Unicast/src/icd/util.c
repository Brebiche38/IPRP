#include "../../inc/icd.h"
#include "../../inc/global.h"
#include "../../inc/receiver.h"

extern volatile iprp_host_t this;

int send_cap(iprp_host_t *sender, int socket) {
	/* TODO stub */
	iprp_ctlmsg_t msg;

	msg.secret = IPRP_CTLMSG_SECRET;
	msg.msg_type = IPRP_CAP;

	printf("in send cap: %d\n", this.nb_ifaces);
	
	msg.message.cap_message.iprp_version = IPRP_VERSION;
	msg.message.cap_message.receiver = this;
	msg.message.cap_message.src_port = 1002;
	msg.message.cap_message.dest_port = 1002;

	printf("in send cap: %d\n", msg.message.cap_message.receiver.nb_ifaces);	

	// Send message
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(IPRP_CTL_PORT);
	addr.sin_addr = sender->ifaces[0].addr;

	printf("in send cap: %d\n", msg.message.cap_message.receiver.nb_ifaces);	

	sendto(socket, (void*) &msg, sizeof(msg), 0, (struct sockaddr*) &addr, sizeof(addr));

	return 0;
}

// No unicast
int send_ack() {
	/* TODO stub */
	return 0;
}

int get_receiver_from_cap(iprp_host_t *recv, iprp_capmsg_t* msg) {
	*recv = msg->receiver;
	return 0;
}