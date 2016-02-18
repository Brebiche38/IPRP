/**\file receiver.h
 * Header file for receiver.c
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#ifndef __IPRP_RECEIVER_
#define __IPRP_RECEIVER_

#include "global.h"

#define IPRP_ACTIVESENDERS_FILE "files/activesenders.iprp"
#define IPRP_ACTIVESENDERS_LINE_LENGTH 80
#define IPRP_ACTIVESENDERS_MAX_SENDERS 256

#define IPRP_DD_MAX_LOST_PACKETS 1024

typedef struct iprp_receiver_link iprp_receiver_link_t;

typedef struct iprp_activesenders_entry {
	iprp_host_t host;
	uint32_t last_seen;
} iprp_as_entry_t;

struct iprp_receiver_link {
	// Info (fixed) vars
	struct in_addr src_addr;
	uint16_t src_port;
	char snsid[20];
	// State (variable) vars
	uint32_t list_sn[IPRP_DD_MAX_LOST_PACKETS];
	uint32_t high_sn;
	time_t last_seen;
};

/* Library functions */
int receiver_init();
int get_active_senders(iprp_host_t *buf, size_t size, int flags);
//int is_active_sender(iprp_sender_t *sender);
//void set_sender_ack(iprp_sender_t *sender);

/* Utility functions */
int parse_as_entry(char *line, iprp_as_entry_t *entry);
int get_as_entry(char *line, iprp_as_entry_t *entry);
int find_sender(iprp_host_t *sender, iprp_host_t *buf);

/* flags */
enum active_senders_flags {
	IPRP_AS_ALL,
	IPRP_AS_NOACK,
	IPRP_AS_ACK
};

#endif /* __IPRP_RECEIVER_ */