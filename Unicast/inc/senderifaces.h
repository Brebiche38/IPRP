#ifndef __IPRP_SENDERIFACES_
#define __IPRP_SENDERIFACES_

#include <time.h>
#include <netinet/in.h>

#include "global.h"

#define IPRP_SI_FILE "files/senderifaces.iprp"

typedef struct {
	struct in_addr sender_addr;
	struct in_addr group_addr;
	int nb_ifaces;
	iprp_iface_t ifaces[IPRP_MAX_IFACE];
	struct in_addr host_addr[IPRP_MAX_IFACE];
	time_t last_seen;
} iprp_sender_ifaces_t;

void senderifaces_store(const char* path, const int count, const iprp_sender_ifaces_t* senders);
int senderifaces_load(const char *path, int* count, iprp_sender_ifaces_t** senders);

#endif /* __IPRP_SENDERIFACES_ */