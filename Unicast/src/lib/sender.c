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
#include <strings.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#include "../../inc/global.h"
#include "../../inc/sender.h"

int peerbase_store(const char* path, iprp_peerbase_t *base) {
	if (path == NULL | base == NULL) return IPRP_ERR_NULLPTR;

	// Save file
	FILE* writer = fopen(path, "w");
	printf("%s\n", path);
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
		printf("%s\n", path);
		ERR("Unable to read peerbase file", errno);
	}

	fread(base, sizeof(iprp_peerbase_t), 1, reader);

	fclose(reader);

	return 0;
}