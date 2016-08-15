/**\file global.h
 * Global definitions
 * 
 * \author Loic Ottet (loic.ottet@epfl.ch)
 */

#ifndef __IPRP_GLOBAL_
#define __IPRP_GLOBAL_

#include <stdint.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include <linux/netfilter.h>
#include <libnetfilter_queue/libnetfilter_queue.h>

#include "debug.h"

#define IPRP_PKTBUF_SIZE 4096
#define IPRP_NFQUEUE_MAX_LENGTH 100
#define IPRP_SNSID_SIZE 20

#define IPRP_VERSION 1
#define IPRP_CTL_PORT 1000
#define IPRP_DATA_PORT 1001
#define IPRP_MAX_IFACE 4 // TODO MAX_IFACE = MAX_INDS (one-to-one)
#define IPRP_MAX_INDS 16
#define IPRP_PATH_LENGTH 50

#define IPRP_ISD_BINARY_LOC "bin/isd"
#define IPRP_IRD_BINARY_LOC "bin/ird"
#define IPRP_IMD_BINARY_LOC "bin/imd"

#define IPRP_IFACE_NAME_LENGTH 10
#define IPRP_MONITORED_PORTS_FILE "ports.txt"
#define IPRP_MAX_MONITORED_PORTS 16

typedef uint32_t iprp_version_t;
typedef uint8_t iprp_ind_t;
typedef uint16_t iprp_ind_bitmap_t;

/* Global structures */
typedef struct {
	uint8_t version;
	unsigned char snsid[IPRP_SNSID_SIZE]; // TODO check if size possible
	uint32_t seq_nb;
	uint16_t dest_port;
	iprp_ind_t ind;
	char hmac[160];
} iprp_header_t;

typedef struct {
	char name[IPRP_IFACE_NAME_LENGTH];
	iprp_ind_t ind;
	struct in_addr addr;
} iprp_iface_t;

typedef struct {
	size_t nb_ifaces;
	iprp_iface_t ifaces[IPRP_MAX_IFACE];
} iprp_host_t;

typedef struct {
	struct in_addr src_addr;
	struct in_addr dest_addr;
	uint16_t src_port;
	uint16_t dest_port;
	char snsid[IPRP_SNSID_SIZE];
} iprp_link_t;

void sockaddr_fill(struct sockaddr_in *sockaddr, struct in_addr addr, uint16_t port);
iprp_ind_bitmap_t ind_match(iprp_host_t *sender, iprp_ind_bitmap_t receiver_inds);

/* NFQueue */
typedef struct {
	struct nfq_handle *handle;
	struct nfq_q_handle *queue;
	int fd;
} iprp_queue_t;

int queue_setup(iprp_queue_t *nfq, int queue_id, nfq_callback *callback);
int get_and_handle(struct nfq_handle *handle, int queue_fd);

/* Time */
void *time_routine(void* arg);

/* List structure */
typedef struct list list_t;
typedef struct list_elem list_elem_t;

struct list_elem {
	void *elem;
	list_elem_t *prev;
	list_elem_t *next;
};

struct list {
	list_elem_t *head;
	list_elem_t *tail;
	size_t size;
	pthread_mutex_t mutex;
};

void list_init(list_t *list);
void list_append(list_t *list, void* value);
void list_delete(list_t *list, list_elem_t *elem);
size_t list_size(list_t *list);
void list_lock(list_t *list);
void list_unlock(list_t *list);

// ATTENTION: doubly linked but not cyclical

// Not used yet
#define LIST_ITERATE(list, type, var, body) 	\
	void *iterator = (list);					\
	while(iterator != NULL) {					\
		type *var = (type *) iterator->elem;	\
		{ body }								\
		iterator = iterator->next;				\
	}


#endif /* __IPRP_GLOBAL_ */