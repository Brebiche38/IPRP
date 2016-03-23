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

#include "../../inc/global.h"
#include "../../inc/receiver.h"

FILE *as_read = NULL; // Only circles around the list
FILE *as_read_some = NULL; // Random access
FILE *as_write = NULL;
//char acks[IPRP_ACTIVESENDERS_MAX_SENDERS];

int activesenders_store(const char* path, int count, iprp_active_sender_t* senders) {
	if (!path || !senders) return IPRP_ERR_NULLPTR;

	FILE* file = fopen(path, "w");
	if (!file) {
		ERR("Unable to open active senders file", errno);
	}

	// TODO prevent concurrent reading (write bit)

	// Write entry count
	fwrite(&count, sizeof(int), 1, file);

	// Write entries
	fwrite(senders, sizeof(iprp_active_sender_t), count, file);

	fflush(writer);

	fclose(writer);

	return 0;
}

int activesenders_load(const char *path, int* count, iprp_active_sender_t** senders) {
	if (!path || !count || ! senders) return IPRP_ERR_NULLPTR;

	FILE* file = fopen(path, "r");
	if (!file) {
		ERR("Unable to open active senders file", errno);
	}

	fread(count, sizeof(int), 1, file);

	*senders = calloc(*count, sizeof(iprp_active_sender_t));

	fread(*senders, sizeof(iprp_active_sender_t), *count, file);

	return 0;
}

