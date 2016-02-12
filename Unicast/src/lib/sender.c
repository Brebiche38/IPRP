/**\file sender.c
 * Sender-side logic (Sender daemons, peer bases)
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#include "../../inc/error.h"
#include "../../inc/log.h"
#include "../../inc/icd.h"
#include "../../inc/sender.h"
#include "../../inc/util.h"

extern iprp_host_t this;

list_t* current_links; // sender_link_t

int sender_init() {
	current_links = malloc(sizeof(list_t));
	if (current_links == NULL) {
		ERR("Unable to allocate", ENOMEM);
	}

	current_links->elem = NULL;
	current_links->next = NULL;
	current_links->prev = NULL;
/*	// Create writer
	pb_write = fopen(IPRP_PEERBASE_FILE, "w");
	if (pb_write == NULL) {
		ERR("Unable to open writer file descriptor for peer base", errno);
	}

	// Create reader
	pb_read = fopen(IPRP_PEERBASE_FILE, "r");
	if (pb_read == NULL) {
		ERR("Unable to open partial reader file descriptor for peer base", errno);
	}
*/
	return 0;
}

iprp_sender_link_t *peerbase_query(iprp_capmsg_t *cap, struct in_addr *dest_addr) {
	if (!cap | !dest_addr) return NULL;

	list_t *iterator = current_links;

	while(iterator) {
		iprp_sender_link_t *link = (iprp_sender_link_t *) iterator->elem;
		if (link->dest_addr.s_addr == dest_addr->s_addr && link->src_port == cap->src_port && link->dest_port == cap->src_port) {
			return link;
		}
		iterator = iterator->next;
	}

	return NULL;
/*
	if (pb_read == NULL) return IPRP_ERR_NOINIT;

	iprp_pb_entry_t buf;

	char line[IPRP_PEERBASE_LINE_LENGTH];
	for (int i = 0; i < IPRP_PEERBASE_MAX_SIZE; ++i) {
		if (fgets(line, IPRP_PEERBASE_LINE_LENGTH, pb_read) == NULL) {
			break;
		}
		parse_pb_entry(line, &buf);

		if (compare_hosts(&buf.host, receiver)) {
			rewind(pb_read);
			return 1;
		}
	}

	rewind(pb_read);
	return 0;
*/
}

int peerbase_insert(iprp_sender_link_t *link, iprp_host_t *receiver, int inds) {
	// 1. Create peer base
	iprp_peerbase_t peerbase;
	bzero(&peerbase, sizeof(iprp_peerbase_t));

	peerbase.link = *link;

	for (int i = 0; i < receiver->nb_ifaces; ++i) {
		int ind = receiver->ifaces[i].ind;
		if (peerbase.paths[ind].active) {
			return IPPR_ERR_MULTIPLE_SAME_IND;
		}

		if (inds & (1 << ind)) {
			iprp_iface_t *iface = get_iface_from_ind(&this, ind);
			if (iface) {
				peerbase.paths[ind].active = true;
				peerbase.paths[ind].iface = *iface;
				peerbase.paths[ind].dest_addr = receiver->ifaces[i].addr;
			}
		}
	}

	peerbase.link.last_cap = time(NULL);

	// TODO 1. Create file for sender deamon
	char path[IPRP_PATH_LENGTH];
	snprintf(path, IPRP_PATH_LENGTH, "files/base_%x.iprp", receiver->id);
	peerbase_store(path, &peerbase);

	// Launch sender deamon and GET PID
	pid_t pid = fork();
	if (pid == -1) {
		ERR("Unable to create sender deamon", errno);
	} else if (!pid) {
		// Child side
		char *args = "";
		execl(IPRP_ISD_BINARY_LOC, "isd", args, NULL);
	} else {
		link->isd_pid = pid;
	}

	// TODO 2. Insert in current links list
	list_append(current_links, link);

	return 0;
/*
	iprp_pb_entry_t buf;

	buf.host = *receiver;
	buf.keep_alive = time(NULL);

	char line[IPRP_PEERBASE_LINE_LENGTH];
	get_pb_entry(line, &buf);

	fputs(line, pb_write);
	fflush(pb_write);

	return 0;
*/
}

int peerbase_update(iprp_sender_link_t *link) {
	iprp_peerbase_t peerbase;
	char path[IPRP_PATH_LENGTH];
	snprintf(path, IPRP_PATH_LENGTH, "files/base_%x.iprp", link->receiver_id);

	int err;

	if ((err = peerbase_load(path, &peerbase))) {
		ERR("Unable to load peerbase", err);
	}

	peerbase.link.last_cap = time(NULL);

	if ((err = peerbase_store(path, &peerbase))) {
		ERR("Unable to store peerbase", err);
	}

	return 0;

/*	if (pb_read == NULL || pb_write == NULL) return IPRP_ERR_NOINIT;

	iprp_pb_entry_t buf;

	char line[IPRP_PEERBASE_LINE_LENGTH];
	for (int i = 0; i < IPRP_PEERBASE_MAX_SIZE; ++i) {
		if (fgets(line, IPRP_PEERBASE_LINE_LENGTH, pb_read) == NULL) {
			break;
		}
		parse_pb_entry(line, &buf);

		if (compare_hosts(&buf.host, receiver)) {
			// Update keep-alive timer
			buf.keep_alive = time(NULL);
			get_pb_entry(line, &buf);

			fputs(line, pb_write);
			fflush(pb_write);
			// TODO delete old line
			rewind(pb_read);
			return 0;
		}
	}
	rewind(pb_read);

	return IPRP_ERR_LOOKUPFAIL;
*/}

int peerbase_store(const char* path, iprp_peerbase_t *base) {
	if (path == NULL | base == NULL) return IPRP_ERR_NULLPTR;

	// Save file
	FILE* writer = fopen(path, "w");
	if (!writer) {
		ERR("Unable to write to peerbase file", errno);
	}

	fwrite(base, sizeof(iprp_peerbase_t), 1, writer);		

	fclose(writer);

	return 0;


/*	// 1. Create string to save
	char file[IPRP_PEERBASE_MAX_SIZE];
	bzero(file, IPRP_PEERBASE_MAX_SIZE);
	int cursor = 0;

	// 1st line
	char ip_buf[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, base->link.dest_addr, ip_buf, INET_ADDRSTRLEN);
	sprintf(file, "%s, %d, %d, %d\n", dest_ip, base->link.src_port, base->link.dest_port, base->last_cap);
	cursor = strlen(file);

	// Next lines
	for (int i = 0; i < IPRP_MAX_INDS; ++i) {
		if (base->paths[i].dest_addr) {
			char src_addr[INET_ADDRSTRLEN];
			char dest_addr[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, base->paths[i].iface.addr, src_addr, INET_ADDRSTRLEN);
			inet_ntop(AF_INET, base->paths[i].dest_addr, src_addr, INET_ADDRSTRLEN);

			sprintf(file + cursor, "%x %s %s %s\n", base->paths[i].iface.ind, src_addr, dest_addr, base->paths[i].iface.name);
			cursor = strlen(file);
		}
	}*/

/*
	char buf[32];
	char *token;


	for (int i = 0; i < IPRP_MAX_ADDR; ++i) {
		// 1. IND
		token = strtok((i == 0) ? line : NULL, ",");
		if (token == NULL) return IPRP_ERR_BADFORMAT;
		strncpy(buf, token, 2);

		entry->host.interfaces[i].ind = strtol(buf, NULL, 16);

		// 2. Address
		token = strtok(NULL, ",");
		if (token == NULL) return IPRP_ERR_BADFORMAT;
		strncpy(buf, token, 16);

		inet_pton(AF_INET, buf, &entry->host.interfaces[i].addr);
	}

	token = strtok(NULL, "\n");
	if (token == NULL) return IPRP_ERR_BADFORMAT;
	strncpy(buf, token, 12);
	entry->keep_alive = strtol(buf, NULL, 10);

	return 0;
*/
}

int peerbase_load(const char *path, iprp_peerbase_t *base) {
	if (path == NULL || base == NULL) return IPRP_ERR_NULLPTR;

	FILE* reader = fopen(path, "r");
	if (!reader) {
		ERR("Unable to read peerbase file", errno);
	}

	fread(base, sizeof(iprp_peerbase_t), 1, reader);

	fclose(reader);

	return 0;


/*	char addr0[INET_ADDRSTRLEN];
	char addr1[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &entry->host.interfaces[0].addr, addr0, INET_ADDRSTRLEN);
	inet_ntop(AF_INET, &entry->host.interfaces[1].addr, addr1, INET_ADDRSTRLEN);

	snprintf(line, IPRP_PEERBASE_LINE_LENGTH, "%x,%s,%x,%s,%d\n", 
		entry->host.interfaces[0].ind, addr0,
		entry->host.interfaces[1].ind, addr1,
		entry->keep_alive);*/

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

	snprintf(&line[cursor], 11, "%d", entry->keep_alive);
	*/
}

int peerbase_cleanup(time_t expiration) {
	// TODO lock
	list_t *iterator = current_links;
	while(iterator) {
		iprp_sender_link_t *link = (iprp_sender_link_t *) iterator->elem;
		if (link->last_cap < expiration) {
			// Delete corresponding connection
			// send SIGTERM to ISD, need to know pid
			kill(link->isd_pid, SIGTERM);
		}
	}
	return 0;
}