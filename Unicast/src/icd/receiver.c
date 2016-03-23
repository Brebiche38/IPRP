#include "../../inc/icd.h"
#include "../../inc/global.h"
#include "../../inc/receiver.h"

extern iprp_host_t this;

int get_active_senders(iprp_active_sender_t **senders) {
	int count;
	int err;
	if (err = (activesenders_load(IPRP_ACTIVESENDERS_FILE, &count, senders))) {
		ERR("Unable to get active senders", err);
	}

	return count;
}

int send_cap(struct in_addr *dest_ip, int socket) {
	/* TODO stub */
	iprp_ctlmsg_t msg;

	msg.secret = IPRP_CTLMSG_SECRET;
	msg.msg_type = IPRP_CAP;
	
	msg.message.cap_message.iprp_version = IPRP_VERSION;
	msg.message.cap_message.receiver = this;
	msg.message.cap_message.src_port = 1002;
	msg.message.cap_message.dest_port = 1002;

	// Send message
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(IPRP_CTL_PORT);
	addr.sin_addr.s_addr = htonl(dest_ip->s_addr);

	printf("%x\n", addr.sin_addr.s_addr);

	sendto(socket, (void*) &msg, sizeof(msg), 0, (struct sockaddr*) &addr, sizeof(addr));

	return 0;
}

size_t get_monitored_ports(uint16_t **table) {
	FILE* ports_file = fopen(IPRP_MONITORED_PORTS_FILE, "r");
	if (!ports_file) {
		ERR("Unable to open ports file", NULL);
	}

	int bytes;
	uint16_t ports[IPRP_MAX_MONITORED_PORTS];

	int num_ports;

	for (int i = 0; i < IPRP_MAX_MONITORED_PORTS; ++i) {
		bytes = fscanf(ports_file, "%d", &ports[i]);
		if (bytes == EOF) {
			num_ports = i;
			break;
		}
	}
	if (bytes != EOF) {
		num_ports = IPRP_MAX_MONITORED_PORTS;
	}

	*table = calloc(num_ports, sizeof (uint16_t));

	for (int i = 0; i < num_ports; ++i) {
		(*table)[i] = ports[i];
	}

	return num_ports;
}