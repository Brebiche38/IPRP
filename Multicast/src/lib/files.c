/**\file receiver.c
 * Receiver-side logic (monitoring and receiving daemons, active senders)
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 * \version alpha
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "global.h"
#include "activesenders.h"
#include "peerbase.h"
#include "senderifaces.h"

int peerbase_store(const char* path, iprp_peerbase_t *base) {
	if (path == NULL | base == NULL) return IPRP_ERR_NULLPTR;

	// Save file
	FILE* writer = fopen(path, "w");
	if (!writer) {
		ERR("Unable to write to peerbase file", errno);
	}

	fwrite(base, sizeof(iprp_peerbase_t), 1, writer);

	fflush(writer);		

	fclose(writer);

	return 0;
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
}

/**
Stores the given active senders to the given file

\param path The file to which the data is stored
\param count The amount of senders to store
\param senders An array of senders of size count to store
\return None
*/
void activesenders_store(const char* path, const int count, const iprp_active_sender_t* senders) {
	// Get file descriptor
	FILE* writer = fopen(path, "w");
	if (!writer) {
		ERR("Unable to open active senders file", errno);
	}

	// TODO prevent concurrent reading (write bit)

	// Write entry count
	fwrite(&count, sizeof(int), 1, writer);

	// Write entries
	if (count > 0) {
		fwrite(senders, sizeof(iprp_active_sender_t), count, writer);
	}

	// Cleanup write
	fflush(writer);
	fclose(writer);
}

/**
Retrieves the active senders list from the given file

\param path The file in which the data is stored
\param count A pointer to which the amount of senders will be stored
\param senders A pointer to which the array of senders will be stored
\return 0 on success, an error code on failure
*/
int activesenders_load(const char *path, int* count, iprp_active_sender_t** senders) {
	if (!count || !senders) return IPRP_ERR_NULLPTR;

	// Get file descriptor
	FILE* reader = fopen(path, "r");
	if (!reader) {
		ERR("Unable to open active senders file", errno);
	}

	// Read entry count
	fread(count, sizeof(int), 1, reader);

	// Read entries
	if (count > 0) {
		*senders = calloc(*count, sizeof(iprp_active_sender_t));
		fread(*senders, sizeof(iprp_active_sender_t), *count, reader);
	}

	// Cleanup read
	fclose(reader);

	return 0;
}

/**
Stores the given sender interfaces to the given file

\param path The file to which the data is stored
\param count The amount of senders to store
\param senders An array of senders of size count to store
\return None
*/
void senderifaces_store(const char* path, const int count, const iprp_sender_ifaces_t* senders) {
	// Get file descriptor
	FILE* writer = fopen(path, "w");
	if (!writer) {
		ERR("Unable to open sender interfaces file", errno);
	}

	// TODO prevent concurrent reading (write bit)

	// Write entry count
	fwrite(&count, sizeof(int), 1, writer);

	// Write entries
	if (count > 0) {
		fwrite(senders, sizeof(iprp_sender_ifaces_t), count, writer);
	}

	// Cleanup write
	fflush(writer);
	fclose(writer);
}

/**
Retrieves the sender interfaces list from the given file

\param path The file in which the data is stored
\param count A pointer to which the amount of senders will be stored
\param senders A pointer to which the array of senders will be stored
\return 0 on success, an error code on failure
*/
int senderifaces_load(const char *path, int* count, iprp_sender_ifaces_t** senders) {
	if (!count || !senders) return IPRP_ERR_NULLPTR;

	// Get file descriptor
	FILE* reader = fopen(path, "r");
	if (!reader) {
		ERR("Unable to open sender interfaces file", errno);
	}

	// Read entry count
	fread(count, sizeof(int), 1, reader);

	// Read entries
	if (count > 0) {
		*senders = calloc(*count, sizeof(iprp_sender_ifaces_t));
		fread(*senders, sizeof(iprp_sender_ifaces_t), *count, reader);
	}

	// Cleanup read
	fclose(reader);

	return 0;
}