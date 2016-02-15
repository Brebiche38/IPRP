/**\file receiver.c
 * Receiver-side logic (monitoring and receiving daemons, active senders)
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#include <errno.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#include "../../inc/receiver.h"
#include "../../inc/types.h"
#include "../../inc/error.h"
#include "../../inc/log.h"

FILE *as_read = NULL; // Only circles around the list
FILE *as_read_some = NULL; // Random access
FILE *as_write = NULL;
//char acks[IPRP_ACTIVESENDERS_MAX_SENDERS];

int receiver_init() {
	as_write = fopen(IPRP_ACTIVESENDERS_FILE, "a");
	if (as_write == NULL) {
		ERR("Unable to open writer file descriptor for active senders", errno);
	}

	as_read_some = fopen(IPRP_ACTIVESENDERS_FILE, "r");
	if (as_write == NULL) {
		ERR("Unable to open partial reader file descriptor for active senders", errno);
	}

	as_read = fopen(IPRP_ACTIVESENDERS_FILE, "r");
	if (as_read == NULL) {
		ERR("Unable to open total reader file descriptor for active senders", errno);
	}

	return 0;
}

int get_active_senders(iprp_host_t *buf, size_t size, int flags) {
	if (as_read == NULL) return IPRP_ERR_NOINIT;
	int count = -1;

	char line[IPRP_ACTIVESENDERS_LINE_LENGTH];
	for (int i = 0; i < size; ++i) {
		if (fgets(line, IPRP_ACTIVESENDERS_LINE_LENGTH, as_read) == NULL) {
			count = i;
			break;
		}

		iprp_as_entry_t entry;
		int err;
		if ((err = parse_as_entry(line, &entry)) != 0) {
			ERR("Active senders list corrupted", err);
		}

		buf[i] = entry.host;
	}

	rewind(as_read);

	return (count == -1) ? size : count;
}

/*
int is_active_sender(iprp_sender_t *sender) {
	if (as_read_some == NULL) return IPRP_ERR_NOINIT;

	iprp_sender_t *buf = NULL;
	find_sender(sender, buf);
	return buf != NULL;
}
*/

/*
// Only multicast
void set_sender_ack(iprp_sender_t *sender) {
	// TODO
	// How? : possible conflict with active senders read => in memory
	// Maybe not necessary for Unicast
}
*/

int parse_as_entry(char *line, iprp_as_entry_t *entry) {
	if (line == NULL || entry == NULL) return IPRP_ERR_NULLPTR;

	char buf[32];
	char *token;


	for (int i = 0; i < 2; ++i) { // TODO variable number of ifaces
		// 1. IND
		token = strtok((i == 0) ? line : NULL, ",");
		if (token == NULL) return IPRP_ERR_BADFORMAT;
		strncpy(buf, token, 2);
		entry->host.ifaces[i].ind = strtol(buf, NULL, 16);

		// 2. Address
		token = strtok(NULL, ",");
		if (token == NULL) return IPRP_ERR_BADFORMAT;
		strncpy(buf, token, 16);
		inet_pton(AF_INET, buf, &entry->host.ifaces[i].addr);
	}

	token = strtok(NULL, "\n");
	if (token == NULL) return IPRP_ERR_BADFORMAT;
	strncpy(buf, token, 12);
	entry->last_seen = strtol(buf, NULL, 10);

	return 0;
}

int find_sender(iprp_host_t *sender, iprp_host_t *buf) {
	// TODO
	return 0;
}

int get_as_entry(char *line, iprp_as_entry_t *entry) {
	char addr0[INET_ADDRSTRLEN];
	char addr1[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &entry->host.ifaces[0].addr, addr0, INET_ADDRSTRLEN);
	inet_ntop(AF_INET, &entry->host.ifaces[1].addr, addr1, INET_ADDRSTRLEN);

	snprintf(line, IPRP_ACTIVESENDERS_LINE_LENGTH, "%x,%s,%x,%s,%d\n", 
		entry->host.ifaces[0].ind, addr0,
		entry->host.ifaces[1].ind, addr1,
		entry->last_seen);

	/*
	int cursor = 0;

	for (int i = 0; i < IPRP_MAX_ADDR; ++i) {
		snprintf(&line[cursor], 2, "%d,", entry->host.interfaces[i].ind);
		cursor += 2;
		char addr[INET_ADDRSTRLEN];
		strncpy(&line[cursor], inet_ntop(AF_INET, &entry->host.interfaces[i].addr, addr, INET_ADDRSTRLEN), strlen(addr));
		cursor += strlen(addr);
		line[cursor] = ",";
		cursor++;
	}

	snprintf(&line[cursor], 11, "%d", entry->last_seen);
	*/

	return 0;
}